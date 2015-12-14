#ifndef __WINE_MONIKER_H__
#define __WINE_MONIKER_H__

#define ICOM_THIS_From_IROTData(class, name) class* This = (class*)(((char*)name)-sizeof(void*))

extern const CLSID CLSID_FileMoniker;
extern const CLSID CLSID_ItemMoniker;
extern const CLSID CLSID_AntiMoniker;
extern const CLSID CLSID_CompositeMoniker;

#endif /* __WINE_MONIKER_H__ */
