package oleaut32;

use strict;

require Exporter;

use wine;
use vars qw(@ISA @EXPORT @EXPORT_OK);

@ISA = qw(Exporter);
@EXPORT = qw();
@EXPORT_OK = qw();

my $module_declarations = {
    "OleCreatePropertyFrame" => ["long",  ["long", "long", "long", "str", "long", "ptr", "long", "ptr", "long", "long", "ptr"]],
    "OleCreatePropertyFrameIndirect" => ["long",  ["ptr"]],
    "DllGetClassObject" => ["long",  ["ptr", "ptr", "ptr"]],
    "SysAllocString" => ["wstr",  ["str"]],
    "SysReAllocString" => ["long",  ["ptr", "str"]],
    "SysAllocStringLen" => ["wstr",  ["ptr", "long"]],
    "SysReAllocStringLen" => ["long",  ["ptr", "ptr", "long"]],
    "SysFreeString" => ["void",  ["wstr"]],
    "SysStringLen" => ["long",  ["wstr"]],
    "VariantInit" => ["void",  ["ptr"]],
    "VariantClear" => ["long",  ["ptr"]],
    "VariantCopy" => ["long",  ["ptr", "ptr"]],
    "VariantCopyInd" => ["long",  ["ptr", "ptr"]],
    "VariantChangeType" => ["long",  ["ptr", "ptr", "long", "long"]],
    "VariantTimeToDosDateTime" => ["long",  ["double", "ptr", "ptr"]],
    "DosDateTimeToVariantTime" => ["long",  ["long", "long", "ptr"]],
    "SafeArrayCreate" => ["ptr",  ["long", "long", "ptr"]],
    "SafeArrayDestroy" => ["long",  ["ptr"]],
    "SafeArrayGetDim" => ["long",  ["ptr"]],
    "SafeArrayGetElemsize" => ["long",  ["ptr"]],
    "SafeArrayGetUBound" => ["long",  ["ptr", "long", "ptr"]],
    "SafeArrayGetLBound" => ["long",  ["ptr", "long", "ptr"]],
    "SafeArrayLock" => ["long",  ["ptr"]],
    "SafeArrayUnlock" => ["long",  ["ptr"]],
    "SafeArrayAccessData" => ["long",  ["ptr", "ptr"]],
    "SafeArrayUnaccessData" => ["long",  ["ptr"]],
    "SafeArrayGetElement" => ["long",  ["ptr", "ptr", "ptr"]],
    "SafeArrayPutElement" => ["long",  ["ptr", "ptr", "ptr"]],
    "SafeArrayCopy" => ["long",  ["ptr", "ptr"]],
    "DispGetParam" => ["long",  ["ptr", "long", "long", "ptr", "ptr"]],
    "DispGetIDsOfNames" => ["long",  ["ptr", "ptr", "long", "ptr"]],
    "DispInvoke" => ["long",  ["ptr", "ptr", "long", "long", "ptr", "ptr", "ptr", "ptr"]],
    "CreateDispTypeInfo" => ["long",  ["ptr", "long", "ptr"]],
    "CreateStdDispatch" => ["long",  ["ptr", "ptr", "ptr", "ptr"]],
    "RegisterActiveObject" => ["long",  ["ptr", "ptr", "long", "ptr"]],
    "RevokeActiveObject" => ["long",  ["long", "ptr"]],
    "GetActiveObject" => ["long",  ["ptr", "ptr", "ptr"]],
    "SafeArrayAllocDescriptor" => ["long",  ["long", "ptr"]],
    "SafeArrayAllocData" => ["long",  ["ptr"]],
    "SafeArrayDestroyDescriptor" => ["long",  ["ptr"]],
    "SafeArrayDestroyData" => ["long",  ["ptr"]],
    "SafeArrayRedim" => ["long",  ["ptr", "ptr"]],
    "VarParseNumFromStr" => ["long",  ["ptr", "long", "long", "ptr", "ptr"]],
    "VarNumFromParseNum" => ["long",  ["ptr", "ptr", "long", "ptr"]],
    "VarI2FromUI1" => ["long",  ["long", "ptr"]],
    "VarI2FromI4" => ["long",  ["long", "ptr"]],
    "VarI2FromR4" => ["long",  ["long", "ptr"]],
    "VarI2FromR8" => ["long",  ["double", "ptr"]],
    "VarI2FromCy" => ["long",  ["double", "ptr"]],
    "VarI2FromDate" => ["long",  ["double", "ptr"]],
    "VarI2FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarI2FromBool" => ["long",  ["long", "ptr"]],
    "VarI4FromUI1" => ["long",  ["long", "ptr"]],
    "VarI4FromI2" => ["long",  ["long", "ptr"]],
    "VarI4FromR4" => ["long",  ["long", "ptr"]],
    "VarI4FromR8" => ["long",  ["double", "ptr"]],
    "VarI4FromCy" => ["long",  ["double", "ptr"]],
    "VarI4FromDate" => ["long",  ["double", "ptr"]],
    "VarI4FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarI4FromBool" => ["long",  ["long", "ptr"]],
    "VarR4FromUI1" => ["long",  ["long", "ptr"]],
    "VarR4FromI2" => ["long",  ["long", "ptr"]],
    "VarR4FromI4" => ["long",  ["long", "ptr"]],
    "VarR4FromR8" => ["long",  ["double", "ptr"]],
    "VarR4FromCy" => ["long",  ["double", "ptr"]],
    "VarR4FromDate" => ["long",  ["double", "ptr"]],
    "VarR4FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarR4FromBool" => ["long",  ["long", "ptr"]],
    "SafeArrayGetVarType" => ["long",  ["ptr", "ptr"]],
    "VarR8FromUI1" => ["long",  ["long", "ptr"]],
    "VarR8FromI2" => ["long",  ["long", "ptr"]],
    "VarR8FromI4" => ["long",  ["long", "ptr"]],
    "VarR8FromR4" => ["long",  ["long", "ptr"]],
    "VarR8FromCy" => ["long",  ["double", "ptr"]],
    "VarR8FromDate" => ["long",  ["double", "ptr"]],
    "VarR8FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarR8FromBool" => ["long",  ["long", "ptr"]],
    "VarDateFromUI1" => ["long",  ["long", "ptr"]],
    "VarDateFromI2" => ["long",  ["long", "ptr"]],
    "VarDateFromI4" => ["long",  ["long", "ptr"]],
    "VarDateFromR4" => ["long",  ["long", "ptr"]],
    "VarDateFromR8" => ["long",  ["double", "ptr"]],
    "VarDateFromCy" => ["long",  ["double", "ptr"]],
    "VarDateFromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarDateFromBool" => ["long",  ["long", "ptr"]],
    "VarCyFromUI1" => ["long",  ["long", "ptr"]],
    "VarCyFromI2" => ["long",  ["long", "ptr"]],
    "VarCyFromI4" => ["long",  ["long", "ptr"]],
    "VarCyFromR4" => ["long",  ["long", "ptr"]],
    "VarCyFromR8" => ["long",  ["double", "ptr"]],
    "VarCyFromDate" => ["long",  ["double", "ptr"]],
    "VarCyFromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarCyFromBool" => ["long",  ["long", "ptr"]],
    "VarBstrFromUI1" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBstrFromI2" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBstrFromI4" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBstrFromR4" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBstrFromR8" => ["long",  ["double", "long", "long", "ptr"]],
    "VarBstrFromCy" => ["long",  ["double", "long", "long", "ptr"]],
    "VarBstrFromDate" => ["long",  ["double", "long", "long", "ptr"]],
    "VarBstrFromBool" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBoolFromUI1" => ["long",  ["long", "ptr"]],
    "VarBoolFromI2" => ["long",  ["long", "ptr"]],
    "VarBoolFromI4" => ["long",  ["long", "ptr"]],
    "VarBoolFromR4" => ["long",  ["long", "ptr"]],
    "VarBoolFromR8" => ["long",  ["double", "ptr"]],
    "VarBoolFromDate" => ["long",  ["double", "ptr"]],
    "VarBoolFromCy" => ["long",  ["double", "ptr"]],
    "VarBoolFromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarUI1FromI2" => ["long",  ["long", "ptr"]],
    "VarUI1FromI4" => ["long",  ["long", "ptr"]],
    "VarUI1FromR4" => ["long",  ["long", "ptr"]],
    "VarUI1FromR8" => ["long",  ["double", "ptr"]],
    "VarUI1FromCy" => ["long",  ["double", "ptr"]],
    "VarUI1FromDate" => ["long",  ["double", "ptr"]],
    "VarUI1FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarUI1FromBool" => ["long",  ["long", "ptr"]],
    "VariantChangeTypeEx" => ["long",  ["ptr", "ptr", "long", "long", "long"]],
    "SafeArrayPtrOfIndex" => ["long",  ["ptr", "ptr", "ptr"]],
    "SysStringByteLen" => ["long",  ["wstr"]],
    "SysAllocStringByteLen" => ["wstr",  ["str", "long"]],
    "CreateTypeLib" => ["long",  ["long", "str", "ptr"]],
    "LoadTypeLib" => ["long",  ["ptr", "ptr"]],
    "LoadRegTypeLib" => ["long",  ["ptr", "long", "long", "long", "ptr"]],
    "RegisterTypeLib" => ["long",  ["ptr", "ptr", "ptr"]],
    "QueryPathOfRegTypeLib" => ["long",  ["ptr", "long", "long", "long", "ptr"]],
    "LHashValOfNameSys" => ["long",  ["long", "long", "str"]],
    "LHashValOfNameSysA" => ["long",  ["long", "long", "str"]],
    "OaBuildVersion" => ["long",  ["undef"]],
    "LoadTypeLibEx" => ["long",  ["str", "long", "ptr"]],
    "SystemTimeToVariantTime" => ["long",  ["ptr", "ptr"]],
    "VariantTimeToSystemTime" => ["long",  ["double", "ptr"]],
    "UnRegisterTypeLib" => ["long",  ["ptr", "long", "long", "long", "long"]],
    "VarI2FromI1" => ["long",  ["long", "ptr"]],
    "VarI2FromUI2" => ["long",  ["long", "ptr"]],
    "VarI2FromUI4" => ["long",  ["long", "ptr"]],
    "VarI4FromI1" => ["long",  ["long", "ptr"]],
    "VarI4FromUI2" => ["long",  ["long", "ptr"]],
    "VarI4FromUI4" => ["long",  ["long", "ptr"]],
    "VarR4FromI1" => ["long",  ["long", "ptr"]],
    "VarR4FromUI2" => ["long",  ["long", "ptr"]],
    "VarR4FromUI4" => ["long",  ["long", "ptr"]],
    "VarR8FromI1" => ["long",  ["long", "ptr"]],
    "VarR8FromUI2" => ["long",  ["long", "ptr"]],
    "VarR8FromUI4" => ["long",  ["long", "ptr"]],
    "VarDateFromI1" => ["long",  ["long", "ptr"]],
    "VarDateFromUI2" => ["long",  ["long", "ptr"]],
    "VarDateFromUI4" => ["long",  ["long", "ptr"]],
    "VarCyFromI1" => ["long",  ["long", "ptr"]],
    "VarCyFromUI2" => ["long",  ["long", "ptr"]],
    "VarCyFromUI4" => ["long",  ["long", "ptr"]],
    "VarBstrFromI1" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBstrFromUI2" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBstrFromUI4" => ["long",  ["long", "long", "long", "ptr"]],
    "VarBoolFromI1" => ["long",  ["long", "ptr"]],
    "VarBoolFromUI2" => ["long",  ["long", "ptr"]],
    "VarBoolFromUI4" => ["long",  ["long", "ptr"]],
    "VarUI1FromI1" => ["long",  ["long", "ptr"]],
    "VarUI1FromUI2" => ["long",  ["long", "ptr"]],
    "VarUI1FromUI4" => ["long",  ["long", "ptr"]],
    "VarI1FromUI1" => ["long",  ["long", "ptr"]],
    "VarI1FromI2" => ["long",  ["long", "ptr"]],
    "VarI1FromI4" => ["long",  ["long", "ptr"]],
    "VarI1FromR4" => ["long",  ["long", "ptr"]],
    "VarI1FromR8" => ["long",  ["double", "ptr"]],
    "VarI1FromDate" => ["long",  ["double", "ptr"]],
    "VarI1FromCy" => ["long",  ["double", "ptr"]],
    "VarI1FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarI1FromBool" => ["long",  ["long", "ptr"]],
    "VarI1FromUI2" => ["long",  ["long", "ptr"]],
    "VarI1FromUI4" => ["long",  ["long", "ptr"]],
    "VarUI2FromUI1" => ["long",  ["long", "ptr"]],
    "VarUI2FromI2" => ["long",  ["long", "ptr"]],
    "VarUI2FromI4" => ["long",  ["long", "ptr"]],
    "VarUI2FromR4" => ["long",  ["long", "ptr"]],
    "VarUI2FromR8" => ["long",  ["double", "ptr"]],
    "VarUI2FromDate" => ["long",  ["double", "ptr"]],
    "VarUI2FromCy" => ["long",  ["double", "ptr"]],
    "VarUI2FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarUI2FromBool" => ["long",  ["long", "ptr"]],
    "VarUI2FromI1" => ["long",  ["long", "ptr"]],
    "VarUI2FromUI4" => ["long",  ["long", "ptr"]],
    "VarUI4FromUI1" => ["long",  ["long", "ptr"]],
    "VarUI4FromI2" => ["long",  ["long", "ptr"]],
    "VarUI4FromI4" => ["long",  ["long", "ptr"]],
    "VarUI4FromR4" => ["long",  ["long", "ptr"]],
    "VarUI4FromR8" => ["long",  ["double", "ptr"]],
    "VarUI4FromDate" => ["long",  ["double", "ptr"]],
    "VarUI4FromCy" => ["long",  ["double", "ptr"]],
    "VarUI4FromStr" => ["long",  ["ptr", "long", "long", "ptr"]],
    "VarUI4FromBool" => ["long",  ["long", "ptr"]],
    "VarUI4FromI1" => ["long",  ["long", "ptr"]],
    "VarUI4FromUI2" => ["long",  ["long", "ptr"]],
    "DllRegisterServer" => ["long",  ["undef"]],
    "VarDateFromUdate" => ["long",  ["ptr", "long", "ptr"]],
    "VarUdateFromDate" => ["long",  ["double", "long", "ptr"]],
    "DllCanUnloadNow" => ["long",  ["undef"]],
    "SafeArrayCreateVector" => ["ptr",  ["long", "long", "long"]],
    "SafeArrayCopyData" => ["long",  ["ptr", "ptr"]],
    "OleIconToCursor" => ["long",  ["long", "long"]],
    "OleLoadPicture" => ["long",  ["ptr", "long", "long", "ptr", "ptr"]],
    "OleCreatePictureIndirect" => ["long",  ["ptr", "ptr", "long", "ptr"]],
    "OleCreateFontIndirect" => ["long",  ["ptr", "ptr", "ptr"]],
    "OleTranslateColor" => ["long",  ["long", "long", "ptr"]],
    "OleLoadPictureEx" => ["long",  ["ptr", "long", "long", "ptr", "long", "long", "long", "ptr"]],
    "VarBstrCat" => ["long",  ["wstr", "wstr", "ptr"]],
    "VarBstrCmp" => ["long",  ["wstr", "wstr", "long", "long"]]
};

&wine::declare("oleaut32",%$module_declarations);
push @EXPORT, map { "&" . $_; } sort(keys(%$module_declarations));
1;
