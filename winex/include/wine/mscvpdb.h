



#include "pshpack1.h"



struct p_string
{
    unsigned char               namelen;
    char                        name[1];
};

union codeview_type
{
    struct
    {
        unsigned short int      len;
        short int               id;
    } generic;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               attribute;
        short int               type;
    } modifier_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        int                     type;
        short int               attribute;
    } modifier_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               attribute;
        short int               datatype;
        struct p_string         p_name;
    } pointer_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned int            datatype;
        unsigned int            attribute;
        struct p_string         p_name;
    } pointer_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               elemtype;
        short int               idxtype;
        unsigned short int      arrlen;     
#if 0
        struct p_string         p_name;
#endif
    } array_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned int            elemtype;
        unsigned int            idxtype;
        unsigned short int      arrlen;    
#if 0
        struct p_string         p_name;
#endif
    } array_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned int            elemtype;
        unsigned int            idxtype;
        unsigned short int      arrlen;    
#if 0
        char                    name[1];
#endif
    } array_v3;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               n_element;
        short int               fieldlist;
        short int               property;
        short int               derived;
        short int               vshape;
        unsigned short int      structlen;  
#if 0
        struct p_string         p_name;
#endif
    } struct_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               n_element;
        short int               property;
        unsigned int            fieldlist;
        unsigned int            derived;
        unsigned int            vshape;
        unsigned short int      structlen;  
#if 0
        struct p_string         p_name;
#endif
    } struct_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               n_element;
        short int               property;
        unsigned int            fieldlist;
        unsigned int            derived;
        unsigned int            vshape;
        unsigned short int      structlen;  
#if 0
        char                    name[1];
#endif
    } struct_v3;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               count;
        short int               fieldlist;
        short int               property;
        unsigned short int      un_len;     
#if 0
        struct p_string         p_name;
#endif
    } union_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               count;
        short int               property;
        unsigned int            fieldlist;
        unsigned short int      un_len;     
#if 0
        struct p_string         p_name;
#endif
    } union_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               count;
        short int               property;
        unsigned int            fieldlist;
        unsigned short int      un_len;     
#if 0
        char                    name[1];
#endif
    } union_v3;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               count;
        short int               type;
        short int               fieldlist;
        short int               property;
        struct p_string         p_name;
    } enumeration_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               count;
        short int               property;
        unsigned int            type;
        unsigned int            fieldlist;
        struct p_string         p_name;
    } enumeration_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        short int               count;
        short int               property;
        unsigned int            type;
        unsigned int            fieldlist;
        char                    name[1];
    } enumeration_v3;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned short int      rvtype;
        unsigned char           call;
        unsigned char           reserved;
        unsigned short int      params;
        unsigned short int      arglist;
    } procedure_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned int            rvtype;
        unsigned char           call;
        unsigned char           reserved;
        unsigned short int      params;
        unsigned int            arglist;
    } procedure_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned short int      rvtype;
        unsigned short int      class_type;
        unsigned short int      this_type;
        unsigned char           call;
        unsigned char           reserved;
        unsigned short int      params;
        unsigned short int      arglist;
        unsigned int            this_adjust;
    } mfunction_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned                this_type;
        unsigned int            class_type;
        unsigned int            rvtype;
        unsigned char           call;
        unsigned char           reserved;
        unsigned short          params;
        unsigned int            arglist;
        unsigned int            this_adjust;
    } mfunction_v2;
};

union codeview_reftype
{
    struct
    {
        unsigned short int      len;
        short int               id;
    } generic;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned char           list[1];
    } fieldlist;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned char           nbits;
        unsigned char           bitoff;
        unsigned short          type;
    } bitfield_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned int            type;
        unsigned char           nbits;
        unsigned char           bitoff;
    } bitfield_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned short          num;
        unsigned short          args[1];
    } arglist_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned                num;
        unsigned                args[1];
    } arglist_v2;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned short          num;
        unsigned short          drvdcls[1];
    } derived_v1;

    struct
    {
        unsigned short int      len;
        short int               id;
        unsigned                num;
        unsigned                drvdcls[1];
    } derived_v2;
};

union codeview_fieldtype
{
    struct
    {
        short int		id;
    } generic;

    struct
    {
        short int		id;
        short int		type;
        short int		attribute;
        unsigned short int	offset;     
    } bclass_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        unsigned short int	offset;     
    } bclass_v2;

    struct
    {
        short int		id;
        short int		btype;
        short int		vbtype;
        short int		attribute;
        unsigned short int	vbpoff;     
#if 0
        unsigned short int	vboff;      
#endif
    } vbclass_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        btype;
        unsigned int	        vbtype;
        unsigned short int	vbpoff;     
#if 0
        unsigned short int	vboff;      
#endif
    } vbclass_v2;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned short int	value;     
#if 0
        struct p_string         p_name;
#endif
    } enumerate_v1;

   struct
    {
        short int               id;
        short int               attribute;
        unsigned short int      value;     
#if 0
        char                    name[1];
#endif
    } enumerate_v3;

    struct
    {
        short int		id;
        short int		type;
        struct p_string         p_name;
    } friendfcn_v1;

    struct
    {
        short int		id;
        short int		_pad0;
        unsigned int	        type;
        struct p_string         p_name;
    } friendfcn_v2;

    struct
    {
        short int		id;
        short int		type;
        short int		attribute;
        unsigned short int	offset;    
#if 0
        struct p_string         p_name;
#endif
    } member_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        unsigned short int	offset;    
#if 0
        struct p_string         p_name;
#endif
    } member_v2;

    struct
    {
        short int               id;
        short int               attribute;
        unsigned int            type;
        unsigned short int      offset; 
#if 0
        unsigned char           name[1];
#endif
    }
    member_v3;

    struct
    {
        short int		id;
        short int		type;
        short int		attribute;
        struct p_string         p_name;
    } stmember_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        struct p_string         p_name;
    } stmember_v2;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        char                    name[1];
    } stmember_v3;

    struct
    {
        short int		id;
        short int		count;
        short int		mlist;
        struct p_string         p_name;
    } method_v1;

    struct
    {
        short int		id;
        short int		count;
        unsigned int	        mlist;
        struct p_string         p_name;
    } method_v2;

    struct
    {
        short int		id;
        short int		count;
        unsigned int	        mlist;
        char                    name[1];
    } method_v3;

    struct
    {
        short int		id;
        short int		type;
        struct p_string         p_name;
    } nesttype_v1;

    struct
    {
        short int		id;
        short int		_pad0;
        unsigned int	        type;
        struct p_string         p_name;
    } nesttype_v2;

    struct
    {
        short int		id;
        short int		_pad0;
        unsigned int	        type;
        char                    name[1];
    } nesttype_v3;

    struct
    {
        short int		id;
        short int		type;
    } vfunctab_v1;

    struct
    {
        short int		id;
        short int		_pad0;
        unsigned int	        type;
    } vfunctab_v2;

    struct
    {
        short int		id;
        short int		type;
    } friendcls_v1;

    struct
    {
        short int		id;
        short int		_pad0;
        unsigned int	        type;
    } friendcls_v2;

    struct
    {
        short int		id;
        short int		attribute;
        short int		type;
        struct p_string         p_name;
    } onemethod_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int 	        type;
        struct p_string         p_name;
    } onemethod_v2;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int 	        type;
        char                    name[1];
    } onemethod_v3;

    struct
    {
        short int		id;
        short int		attribute;
        short int		type;
        unsigned int	        vtab_offset;
        struct p_string         p_name;
    } onemethod_virt_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        unsigned int	        vtab_offset;
        struct p_string         p_name;
    } onemethod_virt_v2;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        unsigned int	        vtab_offset;
        char                    name[1];
    } onemethod_virt_v3;

    struct
    {
        short int		id;
        short int		type;
        unsigned int	        offset;
    } vfuncoff_v1;

    struct
    {
        short int		id;
        short int		_pad0;
        unsigned int	        type;
        unsigned int	        offset;
    } vfuncoff_v2;

    struct
    {
        short int		id;
        short int		attribute;
        short int		type;
        struct p_string         p_name;
    } nesttypeex_v1;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        struct p_string         p_name;
    } nesttypeex_v2;

    struct
    {
        short int		id;
        short int		attribute;
        unsigned int	        type;
        struct p_string         p_name;
    } membermodify_v2;

};






#define T_NOTYPE            0x0000  
#define T_ABS               0x0001  
#define T_SEGMENT           0x0002  
#define T_VOID              0x0003  
#define T_CURRENCY          0x0004  
#define T_NBASICSTR         0x0005  
#define T_FBASICSTR         0x0006  
#define T_NOTTRANS          0x0007  
#define T_RESERVED          0x0008  
#define T_CHAR              0x0010  
#define T_SHORT             0x0011  
#define T_LONG              0x0012  
#define T_QUAD              0x0013  
#define T_UCHAR             0x0020  
#define T_USHORT            0x0021  
#define T_ULONG             0x0022  
#define T_UQUAD             0x0023  
#define T_BOOL08            0x0030  
#define T_BOOL16            0x0031  
#define T_BOOL32            0x0032  
#define T_BOOL64            0x0033  
#define T_REAL32            0x0040  
#define T_REAL64            0x0041  
#define T_REAL80            0x0042  
#define T_REAL128           0x0043  
#define T_REAL48            0x0044  
#define T_CPLX32            0x0050  
#define T_CPLX64            0x0051  
#define T_CPLX80            0x0052  
#define T_CPLX128           0x0053  
#define T_BIT               0x0060  
#define T_PASCHAR           0x0061  
#define T_RCHAR             0x0070  
#define T_WCHAR             0x0071  
#define T_INT2              0x0072  
#define T_UINT2             0x0073  
#define T_INT4              0x0074  
#define T_UINT4             0x0075  
#define T_INT8              0x0076  
#define T_UINT8             0x0077  



#define T_PVOID             0x0103  
#define T_PRESERVED         0x0108  
#define T_PCHAR             0x0110  
#define T_PSHORT            0x0111  
#define T_PLONG             0x0112  
#define T_PQUAD             0x0113  
#define T_PUCHAR            0x0120  
#define T_PUSHORT           0x0121  
#define T_PULONG            0x0122  
#define T_PUQUAD            0x0123  
#define T_PBOOL08           0x0130  
#define T_PBOOL16           0x0131  
#define T_PBOOL32           0x0132  
#define T_PBOOL64           0x0133  
#define T_PREAL32           0x0140  
#define T_PREAL64           0x0141  
#define T_PREAL80           0x0142  
#define T_PREAL128          0x0143  
#define T_PREAL48           0x0144  
#define T_PCPLX32           0x0150  
#define T_PCPLX64           0x0151  
#define T_PCPLX80           0x0152  
#define T_PCPLX128          0x0153  
#define T_PRCHAR            0x0170  
#define T_PWCHAR            0x0171  
#define T_PINT2             0x0172  
#define T_PUINT2            0x0173  
#define T_PINT4             0x0174  
#define T_PUINT4            0x0175  
#define T_PINT8             0x0176  
#define T_PUINT8            0x0177  



#define T_PFVOID            0x0203  
#define T_PFRESERVED        0x0208  
#define T_PFCHAR            0x0210  
#define T_PFSHORT           0x0211  
#define T_PFLONG            0x0212  
#define T_PFQUAD            0x0213  
#define T_PFUCHAR           0x0220  
#define T_PFUSHORT          0x0221  
#define T_PFULONG           0x0222  
#define T_PFUQUAD           0x0223  
#define T_PFBOOL08          0x0230  
#define T_PFBOOL16          0x0231  
#define T_PFBOOL32          0x0232  
#define T_PFBOOL64          0x0233  
#define T_PFREAL32          0x0240  
#define T_PFREAL64          0x0241  
#define T_PFREAL80          0x0242  
#define T_PFREAL128         0x0243  
#define T_PFREAL48          0x0244  
#define T_PFCPLX32          0x0250  
#define T_PFCPLX64          0x0251  
#define T_PFCPLX80          0x0252  
#define T_PFCPLX128         0x0253  
#define T_PFRCHAR           0x0270  
#define T_PFWCHAR           0x0271  
#define T_PFINT2            0x0272  
#define T_PFUINT2           0x0273  
#define T_PFINT4            0x0274  
#define T_PFUINT4           0x0275  
#define T_PFINT8            0x0276  
#define T_PFUINT8           0x0277  



#define T_PHVOID            0x0303  
#define T_PHRESERVED        0x0308  
#define T_PHCHAR            0x0310  
#define T_PHSHORT           0x0311  
#define T_PHLONG            0x0312  
#define T_PHQUAD            0x0313  
#define T_PHUCHAR           0x0320  
#define T_PHUSHORT          0x0321  
#define T_PHULONG           0x0322  
#define T_PHUQUAD           0x0323  
#define T_PHBOOL08          0x0330  
#define T_PHBOOL16          0x0331  
#define T_PHBOOL32          0x0332  
#define T_PHBOOL64          0x0333  
#define T_PHREAL32          0x0340  
#define T_PHREAL64          0x0341  
#define T_PHREAL80          0x0342  
#define T_PHREAL128         0x0343  
#define T_PHREAL48          0x0344  
#define T_PHCPLX32          0x0350  
#define T_PHCPLX64          0x0351  
#define T_PHCPLX80          0x0352  
#define T_PHCPLX128         0x0353  
#define T_PHRCHAR           0x0370  
#define T_PHWCHAR           0x0371  
#define T_PHINT2            0x0372  
#define T_PHUINT2           0x0373  
#define T_PHINT4            0x0374  
#define T_PHUINT4           0x0375  
#define T_PHINT8            0x0376  
#define T_PHUINT8           0x0377  



#define T_32PVOID           0x0403  
#define T_32PRESERVED       0x0408  
#define T_32PCHAR           0x0410  
#define T_32PSHORT          0x0411  
#define T_32PLONG           0x0412  
#define T_32PQUAD           0x0413  
#define T_32PUCHAR          0x0420  
#define T_32PUSHORT         0x0421  
#define T_32PULONG          0x0422  
#define T_32PUQUAD          0x0423  
#define T_32PBOOL08         0x0430  
#define T_32PBOOL16         0x0431  
#define T_32PBOOL32         0x0432  
#define T_32PBOOL64         0x0433  
#define T_32PREAL32         0x0440  
#define T_32PREAL64         0x0441  
#define T_32PREAL80         0x0442  
#define T_32PREAL128        0x0443  
#define T_32PREAL48         0x0444  
#define T_32PCPLX32         0x0450  
#define T_32PCPLX64         0x0451  
#define T_32PCPLX80         0x0452  
#define T_32PCPLX128        0x0453  
#define T_32PRCHAR          0x0470  
#define T_32PWCHAR          0x0471  
#define T_32PINT2           0x0472  
#define T_32PUINT2          0x0473  
#define T_32PINT4           0x0474  
#define T_32PUINT4          0x0475  
#define T_32PINT8           0x0476  
#define T_32PUINT8          0x0477  



#define T_32PFVOID          0x0503  
#define T_32PFRESERVED      0x0508  
#define T_32PFCHAR          0x0510  
#define T_32PFSHORT         0x0511  
#define T_32PFLONG          0x0512  
#define T_32PFQUAD          0x0513  
#define T_32PFUCHAR         0x0520  
#define T_32PFUSHORT        0x0521  
#define T_32PFULONG         0x0522  
#define T_32PFUQUAD         0x0523  
#define T_32PFBOOL08        0x0530  
#define T_32PFBOOL16        0x0531  
#define T_32PFBOOL32        0x0532  
#define T_32PFBOOL64        0x0533  
#define T_32PFREAL32        0x0540  
#define T_32PFREAL64        0x0541  
#define T_32PFREAL80        0x0542  
#define T_32PFREAL128       0x0543  
#define T_32PFREAL48        0x0544  
#define T_32PFCPLX32        0x0550  
#define T_32PFCPLX64        0x0551  
#define T_32PFCPLX80        0x0552  
#define T_32PFCPLX128       0x0553  
#define T_32PFRCHAR         0x0570  
#define T_32PFWCHAR         0x0571  
#define T_32PFINT2          0x0572  
#define T_32PFUINT2         0x0573  
#define T_32PFINT4          0x0574  
#define T_32PFUINT4         0x0575  
#define T_32PFINT8          0x0576  
#define T_32PFUINT8         0x0577  



#define T_MAXPREDEFINEDTYPE 0x0580  
#define T_MAXBASICTYPE      0x0080  
#define T_BASICTYPE_MASK    0x00ff  
#define T_BASICTYPE_SHIFT   8       
#define T_MODE_MASK         0x0700  
#define T_SIZE_MASK         0x0007  
#define T_TYPE_MASK         0x00f0  


#define T_NEARPTR_BITS      0x0100
#define T_FARPTR_BITS       0x0200
#define T_HUGEPTR_BITS      0x0300
#define T_NEAR32PTR_BITS    0x0400
#define T_FAR32PTR_BITS     0x0500
#define T_NEAR64PTR_BITS    0x0600



#define LF_MODIFIER_V1          0x0001
#define LF_POINTER_V1           0x0002
#define LF_ARRAY_V1             0x0003
#define LF_CLASS_V1             0x0004
#define LF_STRUCTURE_V1         0x0005
#define LF_UNION_V1             0x0006
#define LF_ENUM_V1              0x0007
#define LF_PROCEDURE_V1         0x0008
#define LF_MFUNCTION_V1         0x0009
#define LF_VTSHAPE_V1           0x000a
#define LF_COBOL0_V1            0x000b
#define LF_COBOL1_V1            0x000c
#define LF_BARRAY_V1            0x000d
#define LF_LABEL_V1             0x000e
#define LF_NULL_V1              0x000f
#define LF_NOTTRAN_V1           0x0010
#define LF_DIMARRAY_V1          0x0011
#define LF_VFTPATH_V1           0x0012
#define LF_PRECOMP_V1           0x0013
#define LF_ENDPRECOMP_V1        0x0014
#define LF_OEM_V1               0x0015
#define LF_TYPESERVER_V1        0x0016

#define LF_MODIFIER_V2          0x1001     
#define LF_POINTER_V2           0x1002
#define LF_ARRAY_V2             0x1003
#define LF_CLASS_V2             0x1004
#define LF_STRUCTURE_V2         0x1005
#define LF_UNION_V2             0x1006
#define LF_ENUM_V2              0x1007
#define LF_PROCEDURE_V2         0x1008
#define LF_MFUNCTION_V2         0x1009
#define LF_COBOL0_V2            0x100a
#define LF_BARRAY_V2            0x100b
#define LF_DIMARRAY_V2          0x100c
#define LF_VFTPATH_V2           0x100d
#define LF_PRECOMP_V2           0x100e
#define LF_OEM_V2               0x100f

#define LF_SKIP_V1              0x0200
#define LF_ARGLIST_V1           0x0201
#define LF_DEFARG_V1            0x0202
#define LF_LIST_V1              0x0203
#define LF_FIELDLIST_V1         0x0204
#define LF_DERIVED_V1           0x0205
#define LF_BITFIELD_V1          0x0206
#define LF_METHODLIST_V1        0x0207
#define LF_DIMCONU_V1           0x0208
#define LF_DIMCONLU_V1          0x0209
#define LF_DIMVARU_V1           0x020a
#define LF_DIMVARLU_V1          0x020b
#define LF_REFSYM_V1            0x020c

#define LF_SKIP_V2              0x1200    
#define LF_ARGLIST_V2           0x1201
#define LF_DEFARG_V2            0x1202
#define LF_FIELDLIST_V2         0x1203
#define LF_DERIVED_V2           0x1204
#define LF_BITFIELD_V2          0x1205
#define LF_METHODLIST_V2        0x1206
#define LF_DIMCONU_V2           0x1207
#define LF_DIMCONLU_V2          0x1208
#define LF_DIMVARU_V2           0x1209
#define LF_DIMVARLU_V2          0x120a


#define LF_BCLASS_V1            0x0400
#define LF_VBCLASS_V1           0x0401
#define LF_IVBCLASS_V1          0x0402
#define LF_ENUMERATE_V1         0x0403
#define LF_FRIENDFCN_V1         0x0404
#define LF_INDEX_V1             0x0405
#define LF_MEMBER_V1            0x0406
#define LF_STMEMBER_V1          0x0407
#define LF_METHOD_V1            0x0408
#define LF_NESTTYPE_V1          0x0409
#define LF_VFUNCTAB_V1          0x040a
#define LF_FRIENDCLS_V1         0x040b
#define LF_ONEMETHOD_V1         0x040c
#define LF_VFUNCOFF_V1          0x040d
#define LF_NESTTYPEEX_V1        0x040e
#define LF_MEMBERMODIFY_V1      0x040f

#define LF_BCLASS_V2            0x1400    
#define LF_VBCLASS_V2           0x1401
#define LF_IVBCLASS_V2          0x1402
#define LF_FRIENDFCN_V2         0x1403
#define LF_INDEX_V2             0x1404
#define LF_MEMBER_V2            0x1405
#define LF_STMEMBER_V2          0x1406
#define LF_METHOD_V2            0x1407
#define LF_NESTTYPE_V2          0x1408
#define LF_VFUNCTAB_V2          0x1409
#define LF_FRIENDCLS_V2         0x140a
#define LF_ONEMETHOD_V2         0x140b
#define LF_VFUNCOFF_V2          0x140c
#define LF_NESTTYPEEX_V2        0x140d

#define LF_ENUMERATE_V3         0x1502
#define LF_ARRAY_V3             0x1503
#define LF_CLASS_V3             0x1504
#define LF_STRUCTURE_V3         0x1505
#define LF_UNION_V3             0x1506
#define LF_ENUM_V3              0x1507
#define LF_MEMBER_V3            0x150d
#define LF_STMEMBER_V3          0x150e
#define LF_METHOD_V3            0x150f
#define LF_NESTTYPE_V3          0x1510
#define LF_ONEMETHOD_V3         0x1511

#define LF_NUMERIC              0x8000    
#define LF_CHAR                 0x8000
#define LF_SHORT                0x8001
#define LF_USHORT               0x8002
#define LF_LONG                 0x8003
#define LF_ULONG                0x8004
#define LF_REAL32               0x8005
#define LF_REAL64               0x8006
#define LF_REAL80               0x8007
#define LF_REAL128              0x8008
#define LF_QUADWORD             0x8009
#define LF_UQUADWORD            0x800a
#define LF_REAL48               0x800b
#define LF_COMPLEX32            0x800c
#define LF_COMPLEX64            0x800d
#define LF_COMPLEX80            0x800e
#define LF_COMPLEX128           0x800f
#define LF_VARSTRING            0x8010



union codeview_symbol
{
    struct
    {
        short int	        len;
        short int	        id;
    } generic;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned short	        symtype;
        struct p_string         p_name;
    } data_v1;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        symtype;
	unsigned int	        offset;
	unsigned short	        segment;
        struct p_string         p_name;
    } data_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            symtype;
        unsigned int            offset;
        unsigned short          segment;
        char                    name[1];
    } data_v3;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        pparent;
	unsigned int	        pend;
	unsigned int	        next;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned short	        thunk_len;
	unsigned char	        thtype;
        struct p_string         p_name;
    } thunk_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            pparent;
        unsigned int            pend;
        unsigned int            next;
        unsigned int            offset;
        unsigned short          segment;
        unsigned short          thunk_len;
        unsigned char           thtype;
        char                    name[1];
    } thunk_v3;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        pparent;
	unsigned int	        pend;
	unsigned int	        next;
	unsigned int	        proc_len;
	unsigned int	        debug_start;
	unsigned int	        debug_end;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned short	        proctype;
	unsigned char	        flags;
        struct p_string         p_name;
    } proc_v1;

    struct
    {
	short int	        len;
	short int	        id;
	unsigned int	        pparent;
	unsigned int	        pend;
	unsigned int	        next;
	unsigned int	        proc_len;
	unsigned int	        debug_start;
	unsigned int	        debug_end;
	unsigned int	        proctype;
	unsigned int	        offset;
	unsigned short	        segment;
	unsigned char	        flags;
        struct p_string         p_name;
    } proc_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            pparent;
        unsigned int            pend;
        unsigned int            next;
        unsigned int            proc_len;
        unsigned int            debug_start;
        unsigned int            debug_end;
        unsigned int            proctype;
        unsigned int            offset;
        unsigned short          segment;
        unsigned char           flags;
        char                    name[1];
    } proc_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            symtype;
        unsigned int            offset;
        unsigned short          segment;
        struct p_string         p_name;
    } public_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            symtype;
        unsigned int            offset;
        unsigned short          segment;
        char                    name[1];
    } public_v3;

    struct
    {
	short int	        len;	        
	short int	        id;		
	unsigned int	        offset;	        
	unsigned short	        symtype;
        struct p_string         p_name;
    } stack_v1;

    struct
    {
	short int	        len;	        
	short int	        id;		
	unsigned int	        offset;	        
	unsigned int	        symtype;
        struct p_string         p_name;
    } stack_v2;

    struct
    {
        short int               len;            
        short int               id;             
        int                     offset;         
        unsigned int            symtype;
        char                    name[1];
    } stack_v3;

    struct
    {
	short int	        len;	        
	short int	        id;		
        unsigned short          type;
        unsigned short          reg;
        struct p_string         p_name;
        
    } register_v1;

    struct
    {
	short int	        len;	        
	short int	        id;		
        unsigned int            type;           
        unsigned short          reg;
        struct p_string         p_name;
        
    } register_v2;

    struct
    {
	short int	        len;	        
	short int	        id;		
        unsigned int            type;           
        unsigned short          reg;
        char                    name[1];
        
    } register_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            parent;
        unsigned int            end;
        unsigned int            length;
        unsigned int            offset;
        unsigned short          segment;
        struct p_string         p_name;
    } block_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            parent;
        unsigned int            end;
        unsigned int            length;
        unsigned int            offset;
        unsigned short          segment;
        char                    name[1];
    } block_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            offset;
        unsigned short          segment;
        unsigned char           flags;
        struct p_string         p_name;
    } label_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            offset;
        unsigned short          segment;
        unsigned char           flags;
        char                    name[1];
    } label_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned short          type;
        unsigned short          cvalue;         
#if 0
        struct p_string         p_name;
#endif
    } constant_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned                type;
        unsigned short          cvalue;         
#if 0
        struct p_string         p_name;
#endif
    } constant_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned                type;
        unsigned short          cvalue;
#if 0
        char                    name[1];
#endif
    } constant_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned short          type;
        struct p_string         p_name;
    } udt_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned                type;
        struct p_string         p_name;
    } udt_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            type;
        char                    name[1];
    } udt_v3;

    struct
    {
        short int               len;
        short int               id;
        char                    signature[4];
        struct p_string         p_name;
    } objname_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            unknown;
        struct p_string         p_name;
    } compiland_v1;

    struct
    {
        short int               len;
        short int               id;
        unsigned                unknown1[4];
        unsigned short          unknown2;
        struct p_string         p_name;
    } compiland_v2;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            unknown;
        char                    name[1];
    } compiland_v3;

    struct
    {
        short int               len;
        short int               id;
        unsigned int            offset;
        unsigned short          segment;
    } ssearch_v1;
};

#define S_COMPILAND_V1  0x0001
#define S_REGISTER_V1   0x0002
#define S_CONSTANT_V1   0x0003
#define S_UDT_V1        0x0004
#define S_SSEARCH_V1    0x0005
#define S_END_V1        0x0006
#define S_SKIP_V1       0x0007
#define S_CVRESERVE_V1  0x0008
#define S_OBJNAME_V1    0x0009
#define S_ENDARG_V1     0x000a
#define S_COBOLUDT_V1   0x000b
#define S_MANYREG_V1    0x000c
#define S_RETURN_V1     0x000d
#define S_ENTRYTHIS_V1  0x000e

#define S_BPREL_V1      0x0200
#define S_LDATA_V1      0x0201
#define S_GDATA_V1      0x0202
#define S_PUB_V1        0x0203
#define S_LPROC_V1      0x0204
#define S_GPROC_V1      0x0205
#define S_THUNK_V1      0x0206
#define S_BLOCK_V1      0x0207
#define S_WITH_V1       0x0208
#define S_LABEL_V1      0x0209
#define S_CEXMODEL_V1   0x020a
#define S_VFTPATH_V1    0x020b
#define S_REGREL_V1     0x020c
#define S_LTHREAD_V1    0x020d
#define S_GTHREAD_V1    0x020e

#define S_PROCREF_V1    0x0400
#define S_DATAREF_V1    0x0401
#define S_ALIGN_V1      0x0402
#define S_LPROCREF_V1   0x0403

#define S_REGISTER_V2   0x1001 
#define S_CONSTANT_V2   0x1002
#define S_UDT_V2        0x1003
#define S_COBOLUDT_V2   0x1004
#define S_MANYREG_V2    0x1005
#define S_BPREL_V2      0x1006
#define S_LDATA_V2      0x1007
#define S_GDATA_V2      0x1008
#define S_PUB_V2        0x1009
#define S_LPROC_V2      0x100a
#define S_GPROC_V2      0x100b
#define S_VFTTABLE_V2   0x100c
#define S_REGREL_V2     0x100d
#define S_LTHREAD_V2    0x100e
#define S_GTHREAD_V2    0x100f
#if 0
#define S_XXXXXXXXX_32  0x1012  
#endif
#define S_COMPILAND_V2  0x1013

#define S_COMPILAND_V3  0x1101
#define S_THUNK_V3      0x1102
#define S_BLOCK_V3      0x1103
#define S_LABEL_V3      0x1105
#define S_REGISTER_V3   0x1106
#define S_CONSTANT_V3   0x1107
#define S_UDT_V3        0x1108
#define S_BPREL_V3      0x110B
#define S_LDATA_V3      0x110C
#define S_GDATA_V3      0x110D
#define S_PUB_V3        0x110E
#define S_LPROC_V3      0x110F
#define S_GPROC_V3      0x1110
#define S_MSTOOL_V3     0x1116  
#define S_PUB_FUNC1_V3  0x1125  
#define S_PUB_FUNC2_V3  0x1127



union any_size
{
    const char*                 c;
    const unsigned char*        uc;
    const short*                s;
    const int*                  i;
    const unsigned int*         ui;
};

struct startend
{
    unsigned int	        start;
    unsigned int	        end;
};

struct codeview_linetab
{
    unsigned int		nline;
    unsigned int		segno;
    unsigned int		start;
    unsigned int		end;
    unsigned int                source;
    const unsigned short*       linetab;
    const unsigned int*         offtab;
};





struct PDB_FILE
{
    DWORD               size;
    DWORD               unknown;
};

struct PDB_JG_HEADER
{
    CHAR                ident[40];
    DWORD               signature;
    DWORD               block_size;
    WORD                free_list;
    WORD                total_alloc;
    struct PDB_FILE     toc;
    WORD                toc_block[1];
};

struct PDB_DS_HEADER
{
    char                signature[32];
    DWORD               block_size;
    DWORD               unknown1;
    DWORD               num_pages;
    DWORD               toc_size;
    DWORD               unknown2;
    DWORD               toc_page;
};

struct PDB_JG_TOC
{
    DWORD               num_files;
    struct PDB_FILE     file[1];
};

struct PDB_DS_TOC
{
    DWORD               num_files;
    DWORD               file_size[1];
};

struct PDB_JG_ROOT
{
    DWORD               Version;
    DWORD               TimeDateStamp;
    DWORD               Age;
    DWORD               cbNames;
    CHAR                names[1];
};

struct PDB_DS_ROOT
{
    DWORD               Version;
    DWORD               TimeDateStamp;
    DWORD               Age;
    GUID                guid;
    DWORD               cbNames;
    CHAR                names[1];
};

typedef struct _PDB_TYPES_OLD
{
    DWORD       version;
    WORD        first_index;
    WORD        last_index;
    DWORD       type_size;
    WORD        file;
    WORD        pad;
} PDB_TYPES_OLD, *PPDB_TYPES_OLD;

typedef struct _PDB_TYPES
{
    DWORD       version;
    DWORD       type_offset;
    DWORD       first_index;
    DWORD       last_index;
    DWORD       type_size;
    WORD        file;
    WORD        pad;
    DWORD       hash_size;
    DWORD       hash_base;
    DWORD       hash_offset;
    DWORD       hash_len;
    DWORD       search_offset;
    DWORD       search_len;
    DWORD       unknown_offset;
    DWORD       unknown_len;
} PDB_TYPES, *PPDB_TYPES;

typedef struct _PDB_SYMBOL_RANGE
{
    WORD        segment;
    WORD        pad1;
    DWORD       offset;
    DWORD       size;
    DWORD       characteristics;
    WORD        index;
    WORD        pad2;
} PDB_SYMBOL_RANGE, *PPDB_SYMBOL_RANGE;

typedef struct _PDB_SYMBOL_RANGE_EX
{
    WORD        segment;
    WORD        pad1;
    DWORD       offset;
    DWORD       size;
    DWORD       characteristics;
    WORD        index;
    WORD        pad2;
    DWORD       timestamp;
    DWORD       unknown;
} PDB_SYMBOL_RANGE_EX, *PPDB_SYMBOL_RANGE_EX;

typedef struct _PDB_SYMBOL_FILE
{
    DWORD       unknown1;
    PDB_SYMBOL_RANGE range;
    WORD        flag;
    WORD        file;
    DWORD       symbol_size;
    DWORD       lineno_size;
    DWORD       unknown2;
    DWORD       nSrcFiles;
    DWORD       attribute;
    CHAR        filename[1];
} PDB_SYMBOL_FILE, *PPDB_SYMBOL_FILE;

typedef struct _PDB_SYMBOL_FILE_EX
{
    DWORD       unknown1;
    PDB_SYMBOL_RANGE_EX range;
    WORD        flag;
    WORD        file;
    DWORD       symbol_size;
    DWORD       lineno_size;
    DWORD       unknown2;
    DWORD       nSrcFiles;
    DWORD       attribute;
    DWORD       reserved[2];
    CHAR        filename[1];
} PDB_SYMBOL_FILE_EX, *PPDB_SYMBOL_FILE_EX;

typedef struct _PDB_SYMBOL_SOURCE
{
    WORD        nModules;
    WORD        nSrcFiles;
    WORD        table[1];
} PDB_SYMBOL_SOURCE, *PPDB_SYMBOL_SOURCE;

typedef struct _PDB_SYMBOL_IMPORT
{
    DWORD       unknown1;
    DWORD       unknown2;
    DWORD       TimeDateStamp;
    DWORD       Age;
    CHAR        filename[1];
} PDB_SYMBOL_IMPORT, *PPDB_SYMBOL_IMPORT;

typedef struct _PDB_SYMBOLS_OLD
{
    WORD        hash1_file;
    WORD        hash2_file;
    WORD        gsym_file;
    WORD        pad;
    DWORD       module_size;
    DWORD       offset_size;
    DWORD       hash_size;
    DWORD       srcmodule_size;
} PDB_SYMBOLS_OLD, *PPDB_SYMBOLS_OLD;

typedef struct _PDB_SYMBOLS
{
    DWORD       signature;
    DWORD       version;
    DWORD       unknown;
    DWORD       hash1_file;
    DWORD       hash2_file;
    DWORD       gsym_file;
    DWORD       module_size;
    DWORD       offset_size;
    DWORD       hash_size;
    DWORD       srcmodule_size;
    DWORD       pdbimport_size;
    DWORD       resvd[5];
} PDB_SYMBOLS, *PPDB_SYMBOLS;

#include "poppack.h"



typedef struct
{
    DWORD  from;
    DWORD  to;
} OMAP_DATA;

struct msc_debug_info
{
    struct module*              module;
    int			        nsect;
    const IMAGE_SECTION_HEADER* sectp;
    int			        nomap;
    const OMAP_DATA*            omapp;
    const BYTE*                 root;
};


extern BOOL coff_process_info(const struct msc_debug_info* msc_dbg);



#define sstModule      0x120
#define sstTypes       0x121
#define sstPublic      0x122
#define sstPublicSym   0x123
#define sstSymbols     0x124
#define sstAlignSym    0x125
#define sstSrcLnSeg    0x126
#define sstSrcModule   0x127
#define sstLibraries   0x128
#define sstGlobalSym   0x129
#define sstGlobalPub   0x12a
#define sstGlobalTypes 0x12b
#define sstMPC         0x12c
#define sstSegMap      0x12d
#define sstSegName     0x12e
#define sstPreComp     0x12f
#define sstFileIndex   0x133
#define sstStaticSym   0x134


typedef struct OMFSignature
{
    char        Signature[4];
    long        filepos;
} OMFSignature;

typedef struct OMFSignatureRSDS
{
    char        Signature[4];
    GUID        guid;
    DWORD       unknown;
    CHAR        name[1];
} OMFSignatureRSDS;

typedef struct _CODEVIEW_PDB_DATA
{
    char        Signature[4];
    long        filepos;
    DWORD       timestamp;
    DWORD       unknown;
    CHAR        name[1];
} CODEVIEW_PDB_DATA, *PCODEVIEW_PDB_DATA;

typedef struct OMFDirHeader
{
    WORD        cbDirHeader;
    WORD        cbDirEntry;
    DWORD       cDir;
    DWORD       lfoNextDir;
    DWORD       flags;
} OMFDirHeader;

typedef struct OMFDirEntry
{
    WORD        SubSection;
    WORD        iMod;
    DWORD       lfo;
    DWORD       cb;
} OMFDirEntry;



typedef struct OMFSegDesc
{
    WORD        Seg;
    WORD        pad;
    DWORD       Off;
    DWORD       cbSeg;
} OMFSegDesc;

typedef struct OMFModule
{
    WORD        ovlNumber;
    WORD        iLib;
    WORD        cSeg;
    char        Style[2];

} OMFModule;

typedef struct OMFGlobalTypes
{
    DWORD       flags;
    DWORD       cTypes;

} OMFGlobalTypes;




typedef struct OMFSymHash
{
    unsigned short  symhash;
    unsigned short  addrhash;
    unsigned long   cbSymbol;
    unsigned long   cbHSym;
    unsigned long   cbHAddr;
} OMFSymHash;



typedef struct OMFSegMapDesc
{
    unsigned short  flags;
    unsigned short  ovl;
    unsigned short  group;
    unsigned short  frame;
    unsigned short  iSegName;
    unsigned short  iClassName;
    unsigned long   offset;
    unsigned long   cbSeg;
} OMFSegMapDesc;

typedef struct OMFSegMap
{
    unsigned short  cSeg;
    unsigned short  cSegLog;

} OMFSegMap;




typedef struct OMFSourceLine
{
    unsigned short  Seg;
    unsigned short  cLnOff;
    unsigned long   offset[1];
    unsigned short  lineNbr[1];
} OMFSourceLine;

typedef struct OMFSourceFile
{
    unsigned short  cSeg;
    unsigned short  reserved;
    unsigned long   baseSrcLn[1];
    unsigned short  cFName;
    char            Name;
} OMFSourceFile;

typedef struct OMFSourceModule
{
    unsigned short  cFile;
    unsigned short  cSeg;
    unsigned long   baseSrcFile[1];
} OMFSourceModule;
