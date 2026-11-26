/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* Copyright 2026 XxXJoker13XxX and contributors */
/*
 * JKPro online: a synthetic IPv4 adapter for Wine's iphlpapi on the PS4.
 *
 * Wine on the console has no Nsi device (nsiproxy has no Unix side there), so every
 * NsiAllocateAndGetTable call fails and GetAdaptersInfo, GetIpAddrTable and
 * GetIpForwardTable report no network. PES 2013 then shows "make sure your network
 * settings are configured correctly" after its first server reply. When the Nsi call
 * fails, these functions answer with one Ethernet adapter that carries the console's
 * real local IPv4 address (the source address of a UDP route; no packet is sent), a
 * /24 mask and a default route through x.y.z.1. With a working Nsi device (a PC)
 * nothing changes. Included at the end of iphlpapi_main.c's includes.
 */

/* The interface index of adapters_addresses_ps4 and GetBestInterfaceEx (1 is the loopback). */
#define PW_SYNTH_IFINDEX 2

static DWORD pw_synth_addr;   /* network order, 0 = not known yet / none */
static DWORD pw_synth_netmask, pw_synth_router;   /* from WINE_PS4_NETCFG, 0 = default */

static BOOL pw_synth_ipv4( const char *text, IN_ADDR *addr )
{
    return text && text[0] && !RtlIpv4StringToAddressA( text, TRUE, NULL, addr );
}

/* The title's snapshot of the console's IPv4 configuration (as adapters_addresses_ps4 reads it):
 * "address,netmask,gateway,dns1,dns2,mtu". */
static BOOL pw_synth_netcfg( void )
{
    char config[160], *f[3], *c = config, *comma;
    IN_ADDR a;
    DWORD length = GetEnvironmentVariableA( "WINE_PS4_NETCFG", config, sizeof(config) );
    int i;
    if (!length || length >= sizeof(config)) return FALSE;
    for (i = 0; i < 3; i++)
    {
        f[i] = c;
        if (!(comma = strchr( c, ',' ))) return FALSE;
        *comma = 0;
        c = comma + 1;
    }
    if (!pw_synth_ipv4( f[0], &a ) || !a.s_addr) return FALSE;
    pw_synth_addr = a.s_addr;
    if (pw_synth_ipv4( f[1], &a ) && a.s_addr) pw_synth_netmask = a.s_addr;
    if (pw_synth_ipv4( f[2], &a ) && a.s_addr) pw_synth_router = a.s_addr;
    return TRUE;
}

static BOOL pw_synth_local( DWORD *addr )
{
    typedef int (WINAPI *start_fn)( WORD, WSADATA * );
    typedef SOCKET (WINAPI *socket_fn)( int, int, int );
    typedef int (WINAPI *connect_fn)( SOCKET, const struct sockaddr *, int );
    typedef int (WINAPI *name_fn)( SOCKET, struct sockaddr *, int * );
    typedef int (WINAPI *close_fn)( SOCKET );
    HMODULE ws;
    start_fn start; socket_fn sock; connect_fn conn; name_fn name; close_fn close_s;
    struct sockaddr_in to, me;
    int len = sizeof(me);
    WSADATA wsa;
    SOCKET s;
    BOOL ok;

    if (pw_synth_addr || pw_synth_netcfg()) { *addr = pw_synth_addr; return TRUE; }
    if (!(ws = LoadLibraryW( L"ws2_32.dll" ))) return FALSE;
    start = (start_fn)GetProcAddress( ws, "WSAStartup" );
    sock = (socket_fn)GetProcAddress( ws, "socket" );
    conn = (connect_fn)GetProcAddress( ws, "connect" );
    name = (name_fn)GetProcAddress( ws, "getsockname" );
    close_s = (close_fn)GetProcAddress( ws, "closesocket" );
    if (!start || !sock || !conn || !name || !close_s) return FALSE;
    start( MAKEWORD(2, 2), &wsa );
    if ((s = sock( AF_INET, SOCK_DGRAM, IPPROTO_UDP )) == INVALID_SOCKET) return FALSE;
    memset( &to, 0, sizeof(to) );
    to.sin_family = AF_INET;
    to.sin_port = htons( 53 );
    to.sin_addr.s_addr = htonl( 0x01010101 );   /* 1.1.1.1: only picks the route */
    memset( &me, 0, sizeof(me) );
    ok = !conn( s, (struct sockaddr *)&to, sizeof(to) ) && !name( s, (struct sockaddr *)&me, &len ) &&
         me.sin_addr.s_addr && me.sin_addr.s_addr != htonl( INADDR_LOOPBACK );
    close_s( s );
    if (!ok) return FALSE;
    pw_synth_addr = me.sin_addr.s_addr;
    *addr = pw_synth_addr;
    TRACE( "synthetic adapter %s\n", debugstr_a( inet_ntoa( me.sin_addr ) ) );
    return TRUE;
}

static DWORD pw_synth_mask( void ) { return pw_synth_netmask ? pw_synth_netmask : htonl( 0xffffff00 ); }
static DWORD pw_synth_gw( DWORD addr ) { return pw_synth_router ? pw_synth_router : (addr & pw_synth_mask()) | htonl( 1 ); }

static DWORD pw_synth_ipaddrtable( MIB_IPADDRTABLE *table, ULONG *size )
{
    DWORD addr, needed = FIELD_OFFSET( MIB_IPADDRTABLE, table[1] );
    if (!pw_synth_local( &addr )) return ERROR_NOT_SUPPORTED;
    if (!table || *size < needed) { *size = needed; return ERROR_INSUFFICIENT_BUFFER; }
    memset( table, 0, needed );
    table->dwNumEntries = 1;
    table->table[0].dwAddr = addr;
    table->table[0].dwIndex = PW_SYNTH_IFINDEX;
    table->table[0].dwMask = pw_synth_mask();
    table->table[0].dwBCastAddr = 1;
    table->table[0].dwReasmSize = 0xffff;
    table->table[0].wType = MIB_IPADDR_PRIMARY;
    return ERROR_SUCCESS;
}

static DWORD pw_synth_forwardtable( MIB_IPFORWARDTABLE *table, ULONG *size )
{
    DWORD addr, needed = FIELD_OFFSET( MIB_IPFORWARDTABLE, table[2] );
    MIB_IPFORWARDROW *row;
    if (!pw_synth_local( &addr )) return ERROR_NOT_SUPPORTED;
    if (!table || *size < needed) { *size = needed; return ERROR_INSUFFICIENT_BUFFER; }
    memset( table, 0, needed );
    table->dwNumEntries = 2;
    row = &table->table[0];   /* default route */
    row->dwForwardNextHop = pw_synth_gw( addr );
    row->dwForwardType = MIB_IPROUTE_TYPE_INDIRECT;
    row->dwForwardIfIndex = PW_SYNTH_IFINDEX;
    row->dwForwardProto = MIB_IPPROTO_NETMGMT;
    row->dwForwardMetric1 = 25;
    row = &table->table[1];   /* the local network */
    row->dwForwardDest = addr & pw_synth_mask();
    row->dwForwardMask = pw_synth_mask();
    row->dwForwardNextHop = addr;
    row->dwForwardType = MIB_IPROUTE_TYPE_DIRECT;
    row->dwForwardIfIndex = PW_SYNTH_IFINDEX;
    row->dwForwardProto = MIB_IPPROTO_LOCAL;
    row->dwForwardMetric1 = 25;
    return ERROR_SUCCESS;
}

static void pw_synth_ipstr( IP_ADDR_STRING *s, DWORD addr, DWORD mask )
{
    IN_ADDR a;
    memset( s, 0, sizeof(*s) );
    a.s_addr = addr;
    lstrcpynA( s->IpAddress.String, inet_ntoa( a ), sizeof(s->IpAddress.String) );
    a.s_addr = mask;
    lstrcpynA( s->IpMask.String, inet_ntoa( a ), sizeof(s->IpMask.String) );
}

static DWORD pw_synth_adaptersinfo( IP_ADAPTER_INFO *info, ULONG *size )
{
    DWORD addr, needed = sizeof(*info);
    if (!pw_synth_local( &addr )) return ERROR_NOT_SUPPORTED;
    if (!info || *size < needed) { *size = needed; return ERROR_BUFFER_OVERFLOW; }
    memset( info, 0, needed );
    lstrcpynA( info->AdapterName, "{8A1F3C2E-5B7D-4E9A-9C21-6D0B2F4A7E13}", sizeof(info->AdapterName) );
    lstrcpynA( info->Description, "Ethernet", sizeof(info->Description) );
    info->AddressLength = 6;
    info->Address[0] = 0x02; info->Address[1] = 0x42;
    memcpy( info->Address + 2, &addr, 4 );
    info->Index = PW_SYNTH_IFINDEX;
    info->Type = MIB_IF_TYPE_ETHERNET;
    info->DhcpEnabled = TRUE;
    pw_synth_ipstr( &info->IpAddressList, addr, pw_synth_mask() );
    pw_synth_ipstr( &info->GatewayList, pw_synth_gw( addr ), INADDR_NONE );
    pw_synth_ipstr( &info->DhcpServer, pw_synth_gw( addr ), INADDR_NONE );
    return ERROR_SUCCESS;
}

/* No IPv4 address other than loopback in the Nsi unicast table. */
static BOOL pw_synth_no_ipv4( const struct nsi_ipv4_unicast_key *keys, DWORD count )
{
    DWORD i;
    for (i = 0; i < count; i++)
        if (keys[i].luid.Info.IfType != MIB_IF_TYPE_LOOPBACK && keys[i].addr.s_addr) return FALSE;
    return TRUE;
}

/* GetIfEntry for the synthetic interface; err otherwise. */
static DWORD pw_synth_ifrow( MIB_IFROW *row, DWORD err )
{
    DWORD addr, index = row->dwIndex;
    if (index != PW_SYNTH_IFINDEX || !pw_synth_local( &addr )) return err;
    memset( row, 0, sizeof(*row) );
    lstrcpyW( row->wszName, L"\DEVICE\TCPIP_{8A1F3C2E-5B7D-4E9A-9C21-6D0B2F4A7E13}" );
    row->dwIndex = index;
    row->dwType = MIB_IF_TYPE_ETHERNET;
    row->dwMtu = 1500;
    row->dwSpeed = 1000000000;
    row->dwPhysAddrLen = 6;
    row->bPhysAddr[0] = 0x02; row->bPhysAddr[1] = 0x42;
    memcpy( row->bPhysAddr + 2, &addr, 4 );
    row->dwAdminStatus = MIB_IF_ADMIN_STATUS_UP;
    row->dwOperStatus = MIB_IF_OPER_STATUS_OPERATIONAL;
    row->dwDescrLen = 8;
    memcpy( row->bDescr, "Ethernet", 9 );
    return ERROR_SUCCESS;
}
