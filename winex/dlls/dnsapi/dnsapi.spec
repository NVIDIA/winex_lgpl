name	dnsapi
type	win32
init	DllMain

import kernel32.dll

@ stdcall DnsAcquireContextHandle_A(long ptr ptr) DnsAcquireContextHandle_A
@ stdcall DnsAcquireContextHandle_UTF8(long ptr ptr) DnsAcquireContextHandle_UTF8
@ stdcall DnsAcquireContextHandle_W(long ptr ptr) DnsAcquireContextHandle_W
@ stub DnsAddRecordSet_A
@ stub DnsAddRecordSet_UTF8
@ stub DnsAddRecordSet_W
@ stub DnsAllocateRecord
@ stub DnsApiHeapReset
@ stub DnsAsyncRegisterHostAddrs_A
@ stub DnsAsyncRegisterHostAddrs_UTF8
@ stub DnsAsyncRegisterHostAddrs_W
@ stub DnsAsyncRegisterInit
@ stub DnsAsyncRegisterTerm
@ stub DnsCheckNameCollision_A
@ stub DnsCheckNameCollision_UTF8
@ stub DnsCheckNameCollision_W
@ stub DnsCopyStringEx
@ stub DnsCreateReverseNameStringForIpAddress
@ stub DnsCreateStandardDnsNameCopy
@ stub DnsCreateStringCopy
@ stub DnsDhcpSrvRegisterHostName_W
@ stub DnsDhcpSrvRegisterInit
@ stub DnsDhcpSrvRegisterTerm
@ stub DnsDisableAdapterDomainNameRegistration
@ stub DnsDisableBNodeResolverThread
@ stub DnsDisableDynamicRegistration
@ stub DnsDowncaseDnsNameLabel
@ stub DnsEnableAdapterDomainNameRegistration
@ stub DnsEnableBNodeResolverThread
@ stub DnsEnableDynamicRegistration
@ stdcall DnsExtractRecordsFromMessage_UTF8(ptr long ptr) DnsExtractRecordsFromMessage_UTF8
@ stdcall DnsExtractRecordsFromMessage_W(ptr long ptr) DnsExtractRecordsFromMessage_W
@ stub DnsFindAuthoritativeZone
@ stub DnsFlushResolverCache
@ stub DnsFlushResolverCacheEntry_A
@ stub DnsFlushResolverCacheEntry_UTF8
@ stub DnsFlushResolverCacheEntry_W
@ stub DnsFreeAdapterInformation
@ stub DnsFreeNetworkInformation
@ stub DnsFreeSearchInformation
@ stub DnsGetBufferLengthForStringCopy
@ stub DnsGetCacheDataTable
@ stub DnsGetDnsServerList
@ stub DnsGetDomainName
@ stub DnsGetHostName_A
@ stub DnsGetHostName_UTF8
@ stub DnsGetHostName_W
@ stub DnsGetIpAddressInfoList
@ stub DnsGetIpAddressList
@ stub DnsGetLastServerUpdateIP
@ stub DnsGetMaxNumberOfAddressesToRegister
@ stub DnsGetNetworkInformation
@ stub DnsGetPrimaryDomainName_A
@ stub DnsGetPrimaryDomainName_UTF8
@ stub DnsGetPrimaryDomainName_W
@ stub DnsGetSearchInformation
@ stub DnsIpv6AddressToString
@ stub DnsIpv6StringToAddress
@ stub DnsIsAdapterDomainNameRegistrationEnabled
@ stub DnsIsAMailboxType
@ stub DnsIsDynamicRegistrationEnabled
@ stub DnsIsStatusRcode
@ stub DnsIsStringCountValidForTextType
@ stub DnsMapRcodeToStatus
@ stub DnsModifyRecordSet_A
@ stub DnsModifyRecordSet_UTF8
@ stub DnsModifyRecordSet_W
@ stdcall DnsModifyRecordsInSet_A(ptr ptr long ptr ptr ptr) DnsModifyRecordsInSet_A
@ stdcall DnsModifyRecordsInSet_UTF8(ptr ptr long ptr ptr ptr) DnsModifyRecordsInSet_UTF8
@ stdcall DnsModifyRecordsInSet_W(ptr ptr long ptr ptr ptr) DnsModifyRecordsInSet_W
@ stdcall DnsNameCompare_A(str str) DnsNameCompare_A
@ stub DnsNameCompareEx_A
@ stub DnsNameCompareEx_UTF8
@ stub DnsNameCompareEx_W
@ stdcall DnsNameCompare_W(wstr wstr) DnsNameCompare_W
@ stub DnsNameCopy
@ stub DnsNameCopyAllocate
@ stub DnsNotifyResolver
@ stdcall DnsQuery_A(str long long ptr ptr ptr) DnsQuery_A
@ stdcall DnsQueryConfig(long long wstr ptr ptr ptr) DnsQueryConfig
@ stub DnsQueryEx
@ stdcall DnsQuery_UTF8(str long long ptr ptr ptr) DnsQuery_UTF8
@ stdcall DnsQuery_W(wstr long long ptr ptr ptr) DnsQuery_W
@ stub DnsRecordBuild_UTF8
@ stub DnsRecordBuild_W
@ stdcall DnsRecordCompare(ptr ptr) DnsRecordCompare
@ stdcall DnsRecordCopyEx(ptr long long) DnsRecordCopyEx
@ stdcall DnsRecordListFree(ptr long) DnsRecordListFree
@ stdcall DnsRecordSetCompare(ptr ptr ptr ptr) DnsRecordSetCompare
@ stdcall DnsRecordSetCopyEx(ptr long long) DnsRecordSetCopyEx
@ stdcall DnsRecordSetDetach(ptr) DnsRecordSetDetach
@ stub DnsRecordStringForType
@ stub DnsRecordStringForWritableType
@ stub DnsRecordTypeForName
@ stub DnsRelationalCompare_UTF8
@ stub DnsRelationalCompare_W
@ stdcall DnsReleaseContextHandle(ptr) DnsReleaseContextHandle
@ stub DnsRemoveRegistrations
@ stdcall DnsReplaceRecordSetA(ptr long ptr ptr ptr) DnsReplaceRecordSetA
@ stub DnsReplaceRecordSet_A
@ stdcall DnsReplaceRecordSetUTF8(ptr long ptr ptr ptr) DnsReplaceRecordSetUTF8
@ stub DnsReplaceRecordSet_UTF8
@ stdcall DnsReplaceRecordSetW(ptr long ptr ptr ptr) DnsReplaceRecordSetW
@ stub DnsReplaceRecordSet_W
@ stub DnsServiceNotificationDeregister_A
@ stub DnsServiceNotificationDeregister_UTF8
@ stub DnsServiceNotificationDeregister_W
@ stub DnsServiceNotificationRegister_A
@ stub DnsServiceNotificationRegister_UTF8
@ stub DnsServiceNotificationRegister_W
@ stub DnsSetMaxNumberOfAddressesToRegister
@ stub DnsStatusString
@ stub DnsStringCopyAllocateEx
@ stub DnsUnicodeToUtf8
@ stub DnsUpdate
@ stub DnsUpdateTest_A
@ stub DnsUpdateTest_UTF8
@ stub DnsUpdateTest_W
@ stub DnsUtf8ToUnicode
@ stdcall DnsValidateName_A(str long) DnsValidateName_A
@ stdcall DnsValidateName_UTF8(str long) DnsValidateName_UTF8
@ stdcall DnsValidateName_W(wstr long) DnsValidateName_W
@ stub DnsValidateUtf8Byte
@ stub DnsWinsRecordFlagForString
@ stub DnsWinsRecordFlagString
@ stdcall DnsWriteQuestionToBuffer_UTF8(ptr ptr str long long long) DnsWriteQuestionToBuffer_UTF8
@ stdcall DnsWriteQuestionToBuffer_W(ptr ptr wstr long long long) DnsWriteQuestionToBuffer_W
@ stub DnsWriteReverseNameStringForIpAddress
@ stub GetCurrentTimeInSeconds
