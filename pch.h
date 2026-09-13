#pragma once

#define _WIN32_WINNT 0x0601
#define VC_EXTRALEAN        

#include <afxwin.h>         
#include <afxext.h>         
#include <afxdlgs.h>        
#include <afxdialogex.h>    
#include <afxole.h>         

#define CDialogImpl _TmpAtlDialogImpl
#include <atlbase.h>
#include <atlwin.h>          
#undef CDialogImpl           

#include <afxpropertysheet.h> 

#include <msxml6.h> 