# Automatically generated by make_symbols; DO NOT EDIT!! 
#
# Perl definitions for header file winsock2.h
#


package winsock2;

use strict;

use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);

require Exporter;

@ISA = qw(Exporter);
@EXPORT = qw(
    BASE_PROTOCOL
    CF_ACCEPT
    CF_DEFER
    CF_REJECT
    FD_ACCEPT_BIT
    FD_CLOSE_BIT
    FD_CONNECT_BIT
    FD_MAX_EVENTS
    FD_OOB_BIT
    FD_READ_BIT
    FD_WRITE_BIT
    INCL_WINSOCK_API_PROTOTYPES
    INCL_WINSOCK_API_TYPEDEFS
    IOC_PROTOCOL
    IOC_UNIX
    IOC_VENDOR
    IOC_WS2
    LAYERED_PROTOCOL
    MAX_PROTOCOL_CHAIN
    PVD_CONFIG
    SD_BOTH
    SD_RECEIVE
    SD_SEND
    SG_CONSTRAINED_GROUP
    SG_UNCONSTRAINED_GROUP
    SIO_ADDRESS_LIST_CHANGE
    SIO_ADDRESS_LIST_QUERY
    SIO_ASSOCIATE_HANDLE
    SIO_ENABLE_CIRCULAR_QUEUEING
    SIO_FIND_ROUTE
    SIO_FLUSH
    SIO_GET_BROADCAST_ADDRESS
    SIO_GET_EXTENSION_FUNCTION_POINTER
    SIO_GET_GROUP_QOS
    SIO_GET_INTERFACE_LIST
    SIO_GET_QOS
    SIO_MULTICAST_SCOPE
    SIO_MULTIPOINT_LOOPBACK
    SIO_QUERY_TARGET_PNP_HANDLE
    SIO_ROUTING_INTERFACE_CHANGE
    SIO_ROUTING_INTERFACE_QUERY
    SIO_SET_GROUP_QOS
    SIO_SET_QOS
    SIO_TRANSLATE_HANDLE
    SO_CONDITIONAL_ACCEPT
    SO_GROUP_ID
    SO_GROUP_PRIORITY
    SO_MAX_MSG_SIZE
    SO_PROTOCOL_INFOA
    SO_PROTOCOL_INFOW
    WSAPROTOCOL_LEN
    WSA_FLAG_MULTIPOINT_C_LEAF
    WSA_FLAG_MULTIPOINT_C_ROOT
    WSA_FLAG_MULTIPOINT_D_LEAF
    WSA_FLAG_MULTIPOINT_D_ROOT
    WSA_FLAG_OVERLAPPED
    WSA_INFINITE
    WSA_INVALID_EVENT
    WSA_INVALID_HANDLE
    WSA_INVALID_PARAMETER
    WSA_IO_INCOMPLETE
    WSA_IO_PENDING
    WSA_MAXIMUM_WAIT_EVENTS
    WSA_NOT_ENOUGH_MEMORY
    WSA_OPERATION_ABORTED
    WSA_WAIT_EVENT_0
    WSA_WAIT_FAILED
    WSA_WAIT_IO_COMPLETION
    WSA_WAIT_TIMEOUT
);
@EXPORT_OK = qw();

use constant BASE_PROTOCOL => 1;
use constant CF_ACCEPT => 0;
use constant CF_DEFER => 2;
use constant CF_REJECT => 1;
use constant FD_ACCEPT_BIT => 3;
use constant FD_CLOSE_BIT => 5;
use constant FD_CONNECT_BIT => 4;
use constant FD_MAX_EVENTS => 10;
use constant FD_OOB_BIT => 2;
use constant FD_READ_BIT => 0;
use constant FD_WRITE_BIT => 1;
use constant INCL_WINSOCK_API_PROTOTYPES => 1;
use constant INCL_WINSOCK_API_TYPEDEFS => 0;
use constant IOC_PROTOCOL => 268435456;
use constant IOC_UNIX => 0;
use constant IOC_VENDOR => 402653184;
use constant IOC_WS2 => 134217728;
use constant LAYERED_PROTOCOL => 0;
use constant MAX_PROTOCOL_CHAIN => 7;
use constant PVD_CONFIG => 12289;
use constant SD_BOTH => 2;
use constant SD_RECEIVE => 0;
use constant SD_SEND => 1;
use constant SG_CONSTRAINED_GROUP => 2;
use constant SG_UNCONSTRAINED_GROUP => 1;
use constant SIO_ADDRESS_LIST_CHANGE => 671088663;
use constant SIO_ADDRESS_LIST_QUERY => 1207959574;
use constant SIO_ASSOCIATE_HANDLE => -2013265919;
use constant SIO_ENABLE_CIRCULAR_QUEUEING => 671088642;
use constant SIO_FIND_ROUTE => 1207959555;
use constant SIO_FLUSH => 671088644;
use constant SIO_GET_BROADCAST_ADDRESS => 1207959557;
use constant SIO_GET_EXTENSION_FUNCTION_POINTER => -939524090;
use constant SIO_GET_GROUP_QOS => -939524088;
use constant SIO_GET_INTERFACE_LIST => 1074033791;
use constant SIO_GET_QOS => -939524089;
use constant SIO_MULTICAST_SCOPE => -2013265910;
use constant SIO_MULTIPOINT_LOOPBACK => -2013265911;
use constant SIO_QUERY_TARGET_PNP_HANDLE => 1207959576;
use constant SIO_ROUTING_INTERFACE_CHANGE => -2013265899;
use constant SIO_ROUTING_INTERFACE_QUERY => -939524076;
use constant SIO_SET_GROUP_QOS => -2013265908;
use constant SIO_SET_QOS => -2013265909;
use constant SIO_TRANSLATE_HANDLE => -939524083;
use constant SO_CONDITIONAL_ACCEPT => 12290;
use constant SO_GROUP_ID => 8193;
use constant SO_GROUP_PRIORITY => 8194;
use constant SO_MAX_MSG_SIZE => 8195;
use constant SO_PROTOCOL_INFOA => 8196;
use constant SO_PROTOCOL_INFOW => 8197;
use constant WSAPROTOCOL_LEN => 255;
use constant WSA_FLAG_MULTIPOINT_C_LEAF => 4;
use constant WSA_FLAG_MULTIPOINT_C_ROOT => 2;
use constant WSA_FLAG_MULTIPOINT_D_LEAF => 16;
use constant WSA_FLAG_MULTIPOINT_D_ROOT => 8;
use constant WSA_FLAG_OVERLAPPED => 1;
use constant WSA_INFINITE => -1;
use constant WSA_INVALID_EVENT => 0;
use constant WSA_INVALID_HANDLE => 6;
use constant WSA_INVALID_PARAMETER => 87;
use constant WSA_IO_INCOMPLETE => 996;
use constant WSA_IO_PENDING => 997;
use constant WSA_MAXIMUM_WAIT_EVENTS => 64;
use constant WSA_NOT_ENOUGH_MEMORY => 8;
use constant WSA_OPERATION_ABORTED => 995;
use constant WSA_WAIT_EVENT_0 => 0;
use constant WSA_WAIT_FAILED => -1;
use constant WSA_WAIT_IO_COMPLETION => 192;
use constant WSA_WAIT_TIMEOUT => 258;

1;
