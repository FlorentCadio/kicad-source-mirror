/****************/
/* controle.cpp */
/****************/

#include "fctsys.h"
#include "gr_basic.h"
#include "common.h"
#include "class_drawpanel.h"
#include "eda_dde.h"
#include "wxEeschemaStruct.h"

#include "eeschema_id.h"
#include "general.h"
#include "protos.h"
#include "libeditframe.h"
#include "viewlib_frame.h"
#include "lib_draw_item.h"
#include "lib_pin.h"
#include "sch_sheet.h"
#include "sch_sheet_path.h"
#include "sch_marker.h"
#include "sch_component.h"


/**
 * Function SchematicGeneralLocateAndDisplay
 * Overlaid function
 *  Find the schematic item at cursor position
 *  the priority order is:
 *  - marker
 *  - noconnect
 *  - junction
 *  - wire/bus/entry
 *  - label
 *  - pin
 *  - component
 * @return  an EDA_ITEM pointer on the item or NULL if no item found
 * @param IncludePin = true to search for pins, false to ignore them
 *
 *  For some items, characteristics are displayed on the screen.
 */
SCH_ITEM* SCH_EDIT_FRAME::SchematicGeneralLocateAndDisplay( bool IncludePin )
{
    SCH_ITEM*      DrawStruct;
    wxString       msg;
    wxPoint        mouse_position = GetScreen()->m_MousePosition;
    LIB_PIN*       Pin     = NULL;
    SCH_COMPONENT* LibItem = NULL;

    DrawStruct = SchematicGeneralLocateAndDisplay( mouse_position, IncludePin );

    if( !DrawStruct && ( mouse_position != GetScreen()->m_Curseur) )
    {
        DrawStruct = SchematicGeneralLocateAndDisplay( GetScreen()->m_Curseur, IncludePin );
    }

    if( !DrawStruct )
        return NULL;

    /* Cross probing to pcbnew if a pin or a component is found */
    switch( DrawStruct->Type() )
    {
    case SCH_FIELD_T:
    case LIB_FIELD_T:
        LibItem = (SCH_COMPONENT*) DrawStruct->GetParent();
        SendMessageToPCBNEW( DrawStruct, LibItem );
        break;

    case SCH_COMPONENT_T:
        Pin = GetScreen()->GetPin( GetScreen()->m_Curseur, &LibItem );
        if( Pin )
            break;  // Priority is probing a pin first
        LibItem = (SCH_COMPONENT*) DrawStruct;
        SendMessageToPCBNEW( DrawStruct, LibItem );
        break;

    default:
        Pin = GetScreen()->GetPin( GetScreen()->m_Curseur, &LibItem );
        break;

    case LIB_PIN_T:
        Pin = (LIB_PIN*) DrawStruct;
        break;
    }

    if( Pin )
    {
        /* Force display pin information (the previous display could be a
         * component info) */
        Pin->DisplayInfo( this );

        if( LibItem )
            AppendMsgPanel( LibItem->GetRef( GetSheet() ),
                            LibItem->GetField( VALUE )->m_Text, DARKCYAN );

        // Cross probing:2 - pin found, and send a locate pin command to
        // pcbnew (highlight net)
        SendMessageToPCBNEW( Pin, LibItem );
    }
    return DrawStruct;
}


/**
 * Function SchematicGeneralLocateAndDisplay
 * Overlaid function
 *  Find the schematic item at a given position
 *  the priority order is:
 *  - marker
 *  - noconnect
 *  - junction
 *  - wire/bus/entry
 *  - label
 *  - pin
 *  - component
 * @return  an EDA_ITEM pointer on the item or NULL if no item found
 * @param refpoint = the wxPoint location where to search
 * @param IncludePin = true to search for pins, false to ignore them
 *
 *  For some items, characteristics are displayed on the screen.
 */
SCH_ITEM* SCH_EDIT_FRAME::SchematicGeneralLocateAndDisplay( const wxPoint& refpoint,
                                                            bool           IncludePin )
{
    SCH_ITEM*      DrawStruct;
    LIB_PIN*       Pin;
    SCH_COMPONENT* LibItem;
    wxString       Text;
    wxString       msg;

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), MARKER_T );

    if( DrawStruct )
    {
        DrawStruct->DisplayInfo( this );
        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), NO_CONNECT_T );

    if( DrawStruct )
    {
        ClearMsgPanel();
        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), JUNCTION_T );

    if( DrawStruct )
    {
        ClearMsgPanel();
        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(),
                                         WIRE_T | BUS_T | BUS_ENTRY_T );

    if( DrawStruct )  // We have found a wire: Search for a connected pin at the same location
    {
        Pin = GetScreen()->GetPin( refpoint, &LibItem );

        if( Pin )
        {
            Pin->DisplayInfo( this );

            if( LibItem )
                AppendMsgPanel( LibItem->GetRef( GetSheet() ),
                                LibItem->GetField( VALUE )->m_Text, DARKCYAN );
        }
        else
            ClearMsgPanel();

        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), FIELD_T );

    if( DrawStruct )
    {
        SCH_FIELD* Field = (SCH_FIELD*) DrawStruct;
        LibItem = (SCH_COMPONENT*) Field->GetParent();
        LibItem->DisplayInfo( this );

        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), LABEL_T | TEXT_T );

    if( DrawStruct )
    {
        ClearMsgPanel();
        return DrawStruct;
    }

    /* search for a pin */
    Pin = GetScreen()->GetPin( refpoint, &LibItem );

    if( Pin )
    {
        Pin->DisplayInfo( this );

        if( LibItem )
            AppendMsgPanel( LibItem->GetRef( GetSheet() ),
                            LibItem->GetField( VALUE )->m_Text, DARKCYAN );
        if( IncludePin )
            return LibItem;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), COMPONENT_T );

    if( DrawStruct )
    {
        DrawStruct = LocateSmallestComponent( GetScreen() );
        LibItem    = (SCH_COMPONENT*) DrawStruct;
        LibItem->DisplayInfo( this );
        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), SHEET_T );

    if( DrawStruct )
    {
        ( (SCH_SHEET*) DrawStruct )->DisplayInfo( this );
        return DrawStruct;
    }

    DrawStruct = (SCH_ITEM*) PickStruct( refpoint, GetScreen(), NO_FILTER_T );

    if( DrawStruct )
    {
        return DrawStruct;
    }

    ClearMsgPanel();
    return NULL;
}


void SCH_EDIT_FRAME::GeneralControle( wxDC* aDC, wxPoint aPosition )
{
    wxRealPoint gridSize;
    SCH_SCREEN* screen = GetScreen();
    wxPoint     oldpos;
    int         hotkey = 0;
    wxPoint     pos = aPosition;

    PutOnGrid( &pos );
    oldpos = screen->m_Curseur;
    gridSize = screen->GetGridSize();

    switch( g_KeyPressed )
    {
    case 0:
        break;

    case WXK_NUMPAD8:
    case WXK_UP:
        pos.y -= wxRound( gridSize.y );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD2:
    case WXK_DOWN:
        pos.y += wxRound( gridSize.y );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD4:
    case WXK_LEFT:
        pos.x -= wxRound( gridSize.x );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD6:
    case WXK_RIGHT:
        pos.x += wxRound( gridSize.x );
        DrawPanel->MoveCursor( pos );
        break;

    default:
        hotkey = g_KeyPressed;
        break;
    }

    // Update cursor position.
    screen->m_Curseur = pos;

    if( screen->IsRefreshReq() )
    {
        DrawPanel->Refresh( );
        wxSafeYield();
    }

    if( oldpos != screen->m_Curseur )
    {
        pos = screen->m_Curseur;
        screen->m_Curseur = oldpos;
        DrawPanel->CursorOff( aDC );
        screen->m_Curseur = pos;
        DrawPanel->CursorOn( aDC );

        if( DrawPanel->ManageCurseur )
        {
            DrawPanel->ManageCurseur( DrawPanel, aDC, TRUE );
        }
    }

    if( hotkey )
    {
        if( screen->GetCurItem() && screen->GetCurItem()->m_Flags )
            OnHotKey( aDC, hotkey, screen->GetCurItem() );
        else
            OnHotKey( aDC, hotkey, NULL );
    }

    UpdateStatusBar();    /* Display cursor coordinates info */
    SetToolbars();
}


void LIB_EDIT_FRAME::GeneralControle( wxDC* aDC, wxPoint aPosition )
{
    wxRealPoint gridSize;
    SCH_SCREEN* screen = GetScreen();
    wxPoint     oldpos;
    int         hotkey = 0;
    wxPoint     pos = aPosition;

    PutOnGrid( &pos );
    oldpos = screen->m_Curseur;
    gridSize = screen->GetGridSize();

    switch( g_KeyPressed )
    {
    case 0:
        break;

    case WXK_NUMPAD8:
    case WXK_UP:
        pos.y -= wxRound( gridSize.y );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD2:
    case WXK_DOWN:
        pos.y += wxRound( gridSize.y );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD4:
    case WXK_LEFT:
        pos.x -= wxRound( gridSize.x );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD6:
    case WXK_RIGHT:
        pos.x += wxRound( gridSize.x );
        DrawPanel->MoveCursor( pos );
        break;

    default:
        hotkey = g_KeyPressed;
        break;
    }

    // Update the cursor position.
    screen->m_Curseur = pos;

    if( screen->IsRefreshReq() )
    {
        DrawPanel->Refresh( );
        wxSafeYield();
    }

    if( oldpos != screen->m_Curseur )
    {
        pos = screen->m_Curseur;
        screen->m_Curseur = oldpos;
        DrawPanel->CursorOff( aDC );
        screen->m_Curseur = pos;
        DrawPanel->CursorOn( aDC );

        if( DrawPanel->ManageCurseur )
        {
            DrawPanel->ManageCurseur( DrawPanel, aDC, TRUE );
        }
    }

    if( hotkey )
    {
        if( screen->GetCurItem() && screen->GetCurItem()->m_Flags )
            OnHotKey( aDC, hotkey, screen->GetCurItem() );
        else
            OnHotKey( aDC, hotkey, NULL );
    }

    UpdateStatusBar();
}


void LIB_VIEW_FRAME::GeneralControle( wxDC* aDC, wxPoint aPosition )
{
    wxRealPoint gridSize;
    SCH_SCREEN* screen = GetScreen();
    wxPoint     oldpos;
    int         hotkey = 0;
    wxPoint     pos = aPosition;

    PutOnGrid( &pos );
    oldpos = screen->m_Curseur;
    gridSize = screen->GetGridSize();

    switch( g_KeyPressed )
    {
    case 0:
        break;

    case WXK_NUMPAD8:
    case WXK_UP:
        pos.y -= wxRound( gridSize.y );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD2:
    case WXK_DOWN:
        pos.y += wxRound( gridSize.y );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD4:
    case WXK_LEFT:
        pos.x -= wxRound( gridSize.x );
        DrawPanel->MoveCursor( pos );
        break;

    case WXK_NUMPAD6:
    case WXK_RIGHT:
        pos.x += wxRound( gridSize.x );
        DrawPanel->MoveCursor( pos );
        break;

    default:
        hotkey = g_KeyPressed;
        break;
    }

    // Update cursor position.
    screen->m_Curseur = pos;

    if( screen->IsRefreshReq() )
    {
        DrawPanel->Refresh( );
        wxSafeYield();
    }

    if( oldpos != screen->m_Curseur )
    {
        pos = screen->m_Curseur;
        screen->m_Curseur = oldpos;
        DrawPanel->CursorOff( aDC );
        screen->m_Curseur = pos;
        DrawPanel->CursorOn( aDC );

        if( DrawPanel->ManageCurseur )
        {
            DrawPanel->ManageCurseur( DrawPanel, aDC, TRUE );
        }
    }

    if( hotkey )
    {
        if( screen->GetCurItem() && screen->GetCurItem()->m_Flags )
            OnHotKey( aDC, hotkey, screen->GetCurItem() );
        else
            OnHotKey( aDC, hotkey, NULL );
    }

    UpdateStatusBar();
    SetToolbars();
}
