/*
 * Copyrignt 1998 Bertho A. Stultiens (BS)
 *
 * 16-Apr-1998 BS	- Yeah, dump it to stdout.
 *
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <ctype.h>

#include "wrc.h"
#include "dumpres.h"

/*
 *****************************************************************************
 * Function	: get_typename
 * Syntax	: char *get_typename(resource_t* r)
 * Input	:
 *	r	- Resource description
 * Output	: A pointer to a string representing the resource type
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
char *get_typename(resource_t* r)
{
	switch(r->type){
	case res_acc:	return "ACCELERATOR";
	case res_bmp:	return "BITMAP";
	case res_cur:	return "CURSOR";
	case res_curg:	return "GROUP_CURSOR";
	case res_dlg:	return "DIALOG";
	case res_dlgex:	return "DIALOGEX";
	case res_fnt:	return "FONT";
	case res_ico:	return "ICON";
	case res_icog:	return "GROUP_ICON";
	case res_men:	return "MENU";
	case res_menex:	return "MENUEX";
	case res_rdt:	return "RCDATA";
	case res_stt:	return "STRINGTABLE";
	case res_usr:   return "UserResource";
	case res_msg:	return "MESSAGETABLE";
	case res_ver:	return "VERSIONINFO";
	case res_dlginit: return "DLGINIT";
	case res_toolbar: return "TOOLBAR";
	case res_anicur:  return "CURSOR (animated)";
	case res_aniico:  return "ICON (animated)";
	default: 	return "Unknown";
	}
}

/*
 *****************************************************************************
 * Function	: strncpyWtoA
 * Syntax	: char *strncpyWtoA(char *cs, WCHAR *ws, int maxlen)
 * Input	:
 *	cs	- Pointer to buffer to receive result
 *	ws	- Source wide-string
 *	maxlen	- Max chars to copy
 * Output	: 'cs'
 * Description	: Copy a unicode string to ascii. Copying stops after the
 *		  first occurring '\0' or when maxlen-1 chars are copied. The
 *		  String is always nul terminated.
 * Remarks	: No codepage translation is done.
 *****************************************************************************
*/
char *strncpyWtoA(char *cs, WCHAR *ws, int maxlen)
{
	char *cptr = cs;
	WCHAR *wsMax = ws + maxlen;
	while(ws < wsMax)
	{
		if(*ws > 127)
			printf("***Warning: Unicode string contains non-printable chars***");
		*cptr++ = (char)*ws++;
		maxlen--;
	}
	*cptr = '\0';
	return cs;
}

/*
 *****************************************************************************
 * Function	: print_string
 * Syntax	: void print_string(string_t *str)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
void print_string(string_t *str)
{
	char buffer[512];
	if(!str)
		printf("<none>");
	else if(str->type == str_char)
		printf("\"%s\"", str->str.cstr);
	else
	{
		strncpyWtoA(buffer, str->str.wstr, sizeof(buffer));
		printf("L\"%s\"", buffer);
	}
}

/*
 *****************************************************************************
 * Function	: get_nameid_str
 * Syntax	: char *get_nameid_str(name_id_t *n)
 * Input	:
 *	n	- nameid to convert to text
 * Output	: A pointer to the name.
 * Description	:
 * Remarks	: Not reentrant because of static buffer
 *****************************************************************************
*/
char *get_nameid_str(name_id_t *n)
{
	static char buffer[256];

	if(!n)
		return "<none>";

	if(n->type == name_ord)
	{
		sprintf(buffer, "%d", n->name.i_name);
		return buffer;
	}
	else if(n->type == name_str)
	{
		if(n->name.s_name->type == str_char)
			return n->name.s_name->str.cstr;
		else
		{
			strncpyWtoA(buffer, n->name.s_name->str.wstr, sizeof(buffer));
			return buffer;
		}
	}
	else
		return "Hoooo, report this: wrong type in nameid";
}

/*
 *****************************************************************************
 * Function	: dump_memopt
 * Syntax	: void dump_memopt(DWORD memopt)
 * Input	:
 *	memopt	- flag bits of the options set
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_memopt(DWORD memopt)
{
	printf("Memory/load options: ");
	if(memopt & 0x0040)
		printf("PRELOAD ");
	else
		printf("LOADONCALL ");
	if(memopt & 0x0010)
		printf("MOVEABLE ");
	else
		printf("FIXED ");
	if(memopt & 0x0020)
		printf("PURE ");
	else
		printf("IMPURE ");
	if(memopt & 0x1000)
		printf("DISCARDABLE");
	printf("\n");
}

/*
 *****************************************************************************
 * Function	: dump_lvc
 * Syntax	: void dump_lvc(lvc_t *l)
 * Input	:
 *	l	- pointer to lvc structure
 * Output	:
 * Description	: Dump language, version and characteristics
 * Remarks	:
 *****************************************************************************
*/
static void dump_lvc(lvc_t *l)
{
	if(l->language)
		printf("LANGUAGE %04x, %04x\n", l->language->id, l->language->sub);
	else
		printf("LANGUAGE <not set>\n");

	if(l->version)
		printf("VERSION %08lx\n", *(l->version));
	else
		printf("VERSION <not set>\n");

	if(l->characts)
		printf("CHARACTERISTICS %08lx\n", *(l->characts));
	else
		printf("CHARACTERISTICS <not set>\n");
}

/*
 *****************************************************************************
 * Function	: dump_raw_data
 * Syntax	: void dump_raw_data(raw_data_t *d)
 * Input	:
 *	d	- Raw data descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_raw_data(raw_data_t *d)
{
	unsigned int n;
	int i;
	int j;

	if(!d)
	{
		printf("<none>");
		return;
	}
	printf("Rawdata size: %d\n", d->size);
	if(debuglevel < 2)
		return;

	for(n = 0; n < d->size; n++)
	{
		if((n % 16) == 0)
                {
			if(n)
			{
				printf("- ");
				for(i = 0; i < 16; i++)
					printf("%c", isprint(d->data[n-16+i] & 0xff) ? d->data[n-16+i] : '.');
				printf("\n%08x: ", n);
			}
			else
				printf("%08x: ", n);
                }
		printf("%02x ", d->data[n] & 0xff);
	}
	printf("- ");
	j = d->size % 16;
	if(!j)
		j = 16;
	for(i = 0; i < j; i++)
		printf("%c", isprint(d->data[n-j+i] & 0xff) ? d->data[n-j+i] : '.');
	printf("\n");
}

/*
 *****************************************************************************
 * Function	: dump_accelerator
 * Syntax	: void dump_accelerator(resource_t *acc)
 * Input	:
 *	acc	- Accelerator resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_accelerator(accelerator_t *acc)
{
	event_t *ev = acc->events;

	dump_memopt(acc->memopt);
	dump_lvc(&(acc->lvc));

	printf("Events: %s\n", ev ? "" : "<none>");
	while(ev)
	{
		printf("Key=");
		if(isprint(ev->key))
			printf("\"%c\"", ev->key);
		else if(iscntrl(ev->key))
			printf("\"^%c\"", ev->key +'@');
		else
			printf("\\x%02x", ev->key & 0xff);

		printf(" Id=%d flags=%04x\n", ev->id, ev->flags);
		ev = ev->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_cursor
 * Syntax	: void dump_cursor(cursor_t *cur)
 * Input	:
 *	cur	- Cursor resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_cursor(cursor_t *cur)
{
	printf("Id: %d\n", cur->id);
	printf("Width: %d\n", cur->width);
	printf("Height: %d\n", cur->height);
	printf("X Hotspot: %d\n", cur->xhot);
	printf("Y Hotspot: %d\n", cur->yhot);
	dump_raw_data(cur->data);
}

/*
 *****************************************************************************
 * Function	: dump_cursor_group
 * Syntax	: void dump_cursor_group(cursor_group_t *cur)
 * Input	:
 *	cur	- Cursor group resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_cursor_group(cursor_group_t *curg)
{
	dump_memopt(curg->memopt);
	printf("There are %d cursors in this group\n", curg->ncursor);
}

/*
 *****************************************************************************
 * Function	: dump_icon
 * Syntax	: void dump_icon(icon_t *ico)
 * Input	:
 *	ico	- Icon resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_icon(icon_t *ico)
{
	printf("Id: %d\n", ico->id);
	printf("Width: %d\n", ico->width);
	printf("Height: %d\n", ico->height);
	printf("NColor: %d\n", ico->nclr);
	printf("NPlanes: %d\n", ico->planes);
	printf("NBits: %d\n", ico->bits);
	dump_raw_data(ico->data);
}

/*
 *****************************************************************************
 * Function	: dump_icon_group
 * Syntax	: void dump_icon_group(icon_group_t *ico)
 * Input	:
 *	ico	- Icon group resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_icon_group(icon_group_t *icog)
{
	dump_memopt(icog->memopt);
	printf("There are %d icons in this group\n", icog->nicon);
}

/*
 *****************************************************************************
 * Function	: dump_ani_curico
 * Syntax	: void dump_ani_curico(ani_curico_t *ani)
 * Input	:
 *	ani	- Animated object resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_ani_curico(ani_curico_t *ani)
{
	dump_memopt(ani->memopt);
	dump_lvc(&ani->data->lvc);
	dump_raw_data(ani->data);
}

/*
 *****************************************************************************
 * Function	: dump_font
 * Syntax	: void dump_font(font_t *fnt)
 * Input	:
 *	fnt	- Font resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_font(font_t *fnt)
{
	dump_memopt(fnt->memopt);
	dump_lvc(&(fnt->data->lvc));
	dump_raw_data(fnt->data);
}

/*
 *****************************************************************************
 * Function	: dump_bitmap
 * Syntax	: void dump_bitmap(bitmap_t *bmp)
 * Input	:
 *	bmp	- Bitmap resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_bitmap(bitmap_t *bmp)
{
	dump_memopt(bmp->memopt);
	dump_lvc(&(bmp->data->lvc));
	dump_raw_data(bmp->data);
}

/*
 *****************************************************************************
 * Function	: dump_rcdata
 * Syntax	: void dump_rcdata(rcdata_t *rdt)
 * Input	:
 *	rdt	- RCData resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_rcdata(rcdata_t *rdt)
{
	dump_memopt(rdt->memopt);
	dump_lvc(&(rdt->data->lvc));
	dump_raw_data(rdt->data);
}

/*
 *****************************************************************************
 * Function	: dump_user
 * Syntax	: void dump_user(user_t *usr)
 * Input	:
 *	usr	- User resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_user(user_t *usr)
{
	dump_memopt(usr->memopt);
	dump_lvc(&(usr->data->lvc));
	printf("Class %s\n", get_nameid_str(usr->type));
	dump_raw_data(usr->data);
}

/*
 *****************************************************************************
 * Function	: dump_messagetable
 * Syntax	: void dump_messagetable(messagetable_t *msg)
 * Input	:
 *	msg	- Messagetable resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_messagetable(messagetable_t *msg)
{
	dump_memopt(msg->memopt);
	dump_lvc(&(msg->data->lvc));
	dump_raw_data(msg->data);
}

/*
 *****************************************************************************
 * Function	: dump_stringtable
 * Syntax	: void dump_stringtable(stringtable_t *stt)
 * Input	:
 *	stt	- Stringtable resource descriptor
 * Output	: nop
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_stringtable(stringtable_t *stt)
{
	int i;
	for(; stt; stt = stt->next)
	{
		printf("{\n");
		dump_memopt(stt->memopt);
		dump_lvc(&(stt->lvc));
		for(i = 0; i < stt->nentries; i++)
		{
			printf("Id=%-5d (%d) ", stt->idbase+i, stt->entries[i].id);
			if(stt->entries[i].str)
				print_string(stt->entries[i].str);
			else
				printf("<none>");
			printf("\n");
		}
		printf("}\n");
	}
}

/*
 *****************************************************************************
 * Function	: dump_control
 * Syntax	: void dump_control(control_t *ctrl)
 * Input	:
 *	ctrl	- Control resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_control(control_t *ctrl)
{
	printf("Control {\n\tClass: %s\n", get_nameid_str(ctrl->ctlclass));
	printf("\tText: "); get_nameid_str(ctrl->title); printf("\n");
	printf("\tId: %d\n", ctrl->id);
	printf("\tx, y, w, h: %d, %d, %d, %d\n", ctrl->x, ctrl->y, ctrl->width, ctrl->height);
	if(ctrl->gotstyle)
	{
		assert(ctrl->style != NULL);
		assert(ctrl->style->and_mask == 0);
		printf("\tStyle: %08lx\n", ctrl->style->or_mask);
	}
	if(ctrl->gotexstyle)
	{
		assert(ctrl->exstyle != NULL);
		assert(ctrl->exstyle->and_mask == 0);
		printf("\tExStyle: %08lx\n", ctrl->exstyle->or_mask);
	}
	if(ctrl->gothelpid)
		printf("\tHelpid: %ld\n", ctrl->helpid);
	if(ctrl->extra)
	{
		printf("\t");
		dump_raw_data(ctrl->extra);
	}
	printf("}\n");
}

/*
 *****************************************************************************
 * Function	: dump_dialog
 * Syntax	: void dump_dialog(dialog_t *dlg)
 * Input	:
 *	dlg	- Dialog resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_dialog(dialog_t *dlg)
{
	control_t *c = dlg->controls;

	dump_memopt(dlg->memopt);
	dump_lvc(&(dlg->lvc));
	printf("x, y, w, h: %d, %d, %d, %d\n", dlg->x, dlg->y, dlg->width, dlg->height);
	if(dlg->gotstyle)
	{
		assert(dlg->style != NULL);
		assert(dlg->style->and_mask == 0);
		printf("Style: %08lx\n", dlg->style->or_mask);

	}
	if(dlg->gotexstyle)
	{
		assert(dlg->exstyle != NULL);
		assert(dlg->exstyle->and_mask == 0);
		printf("ExStyle: %08lx\n", dlg->exstyle->or_mask);
	}
	printf("Menu: %s\n", get_nameid_str(dlg->menu));
	printf("Class: %s\n", get_nameid_str(dlg->dlgclass));
	printf("Title: "); print_string(dlg->title); printf("\n");
	printf("Font: ");
	if(!dlg->font)
		printf("<none>\n");
	else
	{
		printf("%d, ", dlg->font->size);
		print_string(dlg->font->name);
		printf("\n");
	}
	while(c)
	{
		dump_control(c);
		c = c->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_dialogex
 * Syntax	: void dump_dialogex(dialogex_t *dlgex)
 * Input	:
 *	dlgex	- DialogEx resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_dialogex(dialogex_t *dlgex)
{
	control_t *c = dlgex->controls;

	dump_memopt(dlgex->memopt);
	dump_lvc(&(dlgex->lvc));
	printf("x, y, w, h: %d, %d, %d, %d\n", dlgex->x, dlgex->y, dlgex->width, dlgex->height);
	if(dlgex->gotstyle)
	{
		assert(dlgex->style != NULL);
		assert(dlgex->style->and_mask == 0);
		printf("Style: %08lx\n", dlgex->style->or_mask);
	}
	if(dlgex->gotexstyle)
	{
		assert(dlgex->exstyle != NULL);
		assert(dlgex->exstyle->and_mask == 0);
		printf("ExStyle: %08lx\n", dlgex->exstyle->or_mask);
	}
	if(dlgex->gothelpid)
		printf("Helpid: %ld\n", dlgex->helpid);
	printf("Menu: %s\n", get_nameid_str(dlgex->menu));
	printf("Class: %s\n", get_nameid_str(dlgex->dlgclass));
	printf("Title: "); print_string(dlgex->title); printf("\n");
	printf("Font: ");
	if(!dlgex->font)
		printf("<none>\n");
	else
	{
		printf("%d, ", dlgex->font->size);
		print_string(dlgex->font->name);
		printf(", %d, %d\n", dlgex->font->weight, dlgex->font->italic);
	}
	while(c)
	{
		dump_control(c);
		c = c->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_menu_item
 * Syntax	: void dump_menu_item(menu_item_t *item)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_menu_item(menu_item_t *item)
{
	while(item)
	{
		if(item->popup)
		{
			printf("POPUP ");
			print_string(item->name);
			printf("\n");
			dump_menu_item(item->popup);
		}
		else
		{
			printf("MENUITEM ");
			if(item->name)
			{
				print_string(item->name);
				printf(", %d, %08lx", item->id, item->state);
			}
			else
				printf("SEPARATOR");
			printf("\n");
		}
		item = item->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_menu
 * Syntax	: void dump_menu(menu_t *men)
 * Input	:
 *	men	- Menu resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_menu(menu_t *men)
{
	dump_memopt(men->memopt);
	dump_lvc(&(men->lvc));
	dump_menu_item(men->items);
}

/*
 *****************************************************************************
 * Function	: dump_menuex_item
 * Syntax	: void dump_menuex_item(menuex_item_t *item)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_menuex_item(menuex_item_t *item)
{
	while(item)
	{
		if(item->popup)
		{
			printf("POPUP ");
			print_string(item->name);
			if(item->gotid)
				printf(", Id=%d", item->id);
			if(item->gottype)
				printf(", Type=%ld", item->type);
			if(item->gotstate)
				printf(", State=%08lx", item->state);
			if(item->gothelpid)
				printf(", HelpId=%d", item->helpid);
			printf("\n");
			dump_menuex_item(item->popup);
		}
		else
		{
			printf("MENUITEM ");
			if(item->name)
			{
				print_string(item->name);
				if(item->gotid)
					printf(", Id=%d", item->id);
				if(item->gottype)
					printf(", Type=%ld", item->type);
				if(item->gotstate)
					printf(", State=%08lx", item->state);
				if(item->gothelpid)
					printf(", HelpId=%d", item->helpid);
			}
			else
				printf("SEPARATOR");
			printf("\n");
		}
		item = item->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_menuex
 * Syntax	: void dump_menuex(dialogex_t *menex)
 * Input	:
 *	menex	- MenuEx resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_menuex(menuex_t *menex)
{
	dump_memopt(menex->memopt);
	dump_lvc(&(menex->lvc));
	dump_menuex_item(menex->items);
}

/*
 *****************************************************************************
 * Function	: dump_ver_value
 * Syntax	: void dump_ver_value(ver_value_t *val)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_ver_block(ver_block_t *);	/* Forward ref */

static void dump_ver_value(ver_value_t *val)
{
	if(val->type == val_str)
	{
		printf("VALUE ");
		print_string(val->key);
		printf(" ");
		print_string(val->value.str);
		printf("\n");
	}
	else if(val->type == val_words)
	{
		int i;
		printf("VALUE");
		print_string(val->key);
		for(i = 0; i < val->value.words->nwords; i++)
			printf(" %04x", val->value.words->words[i]);
		printf("\n");
	}
	else if(val->type == val_block)
	{
		dump_ver_block(val->value.block);
	}
}

/*
 *****************************************************************************
 * Function	: dump_ver_block
 * Syntax	: void dump_ver_block(ver_block_t *blk)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_ver_block(ver_block_t *blk)
{
	ver_value_t *val = blk->values;
	printf("BLOCK ");
	print_string(blk->name);
	printf("\n{\n");
	while(val)
	{
		dump_ver_value(val);
		val = val->next;
	}
	printf("}\n");
}

/*
 *****************************************************************************
 * Function	: dump_versioninfo
 * Syntax	: void dump_versioninfo(versioninfo_t *ver)
 * Input	:
 *	ver	- Versioninfo resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_versioninfo(versioninfo_t *ver)
{
	ver_block_t *blk = ver->blocks;

	dump_lvc(&(ver->lvc));

	if(ver->gotit.fv)
		printf("FILEVERSION %04x, %04x, %04x, %04x\n",
			ver->filever_maj1,
			ver->filever_maj2,
			ver->filever_min1,
			ver->filever_min2);
	if(ver->gotit.pv)
		printf("PRODUCTVERSION %04x, %04x, %04x, %04x\n",
			ver->prodver_maj1,
			ver->prodver_maj2,
			ver->prodver_min1,
			ver->prodver_min2);
	if(ver->gotit.fo)
		printf("FILEOS %08x\n", ver->fileos);
	if(ver->gotit.ff)
		printf("FILEFLAGS %08x\n", ver->fileflags);
	if(ver->gotit.ffm)
		printf("FILEFLAGSMASK %08x\n", ver->fileflagsmask);
	if(ver->gotit.ft)
		printf("FILETYPE %08x\n", ver->filetype);
	if(ver->gotit.fst)
		printf("FILESUBTYPE %08x\n", ver->filesubtype);
	while(blk)
	{
		dump_ver_block(blk);
		blk = blk->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_toolbar_item
 * Syntax	: void dump_toolbar_item(toolbar_item_t *item)
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_toolbar_items(toolbar_item_t *items)
{
	while(items)
	{
	        if(items->id)
			printf("   BUTTON %d", items->id );
		else
	      		printf("   SEPARATOR");

		printf("\n");

		items = items->next;
	}
}

/*
 *****************************************************************************
 * Function	: dump_toolbar
 * Syntax	: void dump_toolbar(toolbar_t *toolbar)
 * Input	:
 *	toolbar	- Toolbar resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_toolbar(toolbar_t *toolbar)
{
	dump_memopt(toolbar->memopt);
	dump_lvc(&(toolbar->lvc));
	dump_toolbar_items(toolbar->items);
}

/*
 *****************************************************************************
 * Function	: dump_dlginit
 * Syntax	: void dump_dlginit(dlginit_t *dit)
 * Input	:
 *	dit	- DlgInit resource descriptor
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
static void dump_dlginit(dlginit_t *dit)
{
	dump_memopt(dit->memopt);
	dump_lvc(&(dit->data->lvc));
	dump_raw_data(dit->data);
}

/*
 *****************************************************************************
 * Function	:
 * Syntax	:
 * Input	:
 * Output	:
 * Description	:
 * Remarks	:
 *****************************************************************************
*/
/*
 *****************************************************************************
 * Function	: dump_resources
 * Syntax	: void dump_resources(resource_t *top)
 * Input	:
 *	top	- Top of the resource tree
 * Output	:
 *	nop
 * Description	: Dump the parsed resource-tree to stdout
 * Remarks	:
 *****************************************************************************
*/
void dump_resources(resource_t *top)
{
	printf("Internal resource-tree dump:\n");
	while(top)
	{
		printf("Resource: %s\nId: %s\n",
		       get_typename(top),
		       get_nameid_str(top->name));
		switch(top->type)
		{
		case res_acc:
			dump_accelerator(top->res.acc);
			break;
		case res_bmp:
			dump_bitmap(top->res.bmp);
			break;
		case res_cur:
			dump_cursor(top->res.cur);
			break;
		case res_curg:
			dump_cursor_group(top->res.curg);
			break;
		case res_dlg:
			dump_dialog(top->res.dlg);
			break;
		case res_dlgex:
			dump_dialogex(top->res.dlgex);
			break;
		case res_fnt:
			dump_font(top->res.fnt);
			break;
		case res_icog:
			dump_icon_group(top->res.icog);
			break;
		case res_ico:
			dump_icon(top->res.ico);
			break;
		case res_men:
			dump_menu(top->res.men);
			break;
		case res_menex:
			dump_menuex(top->res.menex);
			break;
		case res_rdt:
			dump_rcdata(top->res.rdt);
			break;
		case res_stt:
			dump_stringtable(top->res.stt);
			break;
		case res_usr:
			dump_user(top->res.usr);
			break;
		case res_msg:
			dump_messagetable(top->res.msg);
			break;
		case res_ver:
			dump_versioninfo(top->res.ver);
			break;
		case res_dlginit:
			dump_dlginit(top->res.dlgi);
			break;
		case res_toolbar:
			dump_toolbar(top->res.tbt);
			break;
		case res_anicur:
		case res_aniico:
			dump_ani_curico(top->res.ani);
			break;
		default:
			printf("Report this: Unknown resource type parsed %08x\n", top->type);
		}
		printf("\n");
		top = top->next;
	}
}

