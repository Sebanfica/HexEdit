// CompareView.cpp : implementation of the CCompareView class
//
// Copyright (c) 2010 by Andrew W. Phillips. 
//
// No restrictions are placed on the noncommercial use of this code,
// as long as this text (from the above copyright notice to the
// disclaimer below) is preserved.
//
// This code may be redistributed as long as it remains unmodified
// and is not sold for profit without the author's written consent.
//
// This code, or any part of it, may not be used in any software that
// is sold for profit, without the author's written consent.
//
// DISCLAIMER: This file is provided "as is" with no expressed or
// implied warranty. The author accepts no liability for any damage
// or loss of business that this product may cause.
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "HexEdit.h"
#include "HexEditDoc.h"
#include "HexEditView.h"
#include "CompareView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CHexEditApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CCompareView

IMPLEMENT_DYNCREATE(CCompareView, CScrView)

BEGIN_MESSAGE_MAP(CCompareView, CScrView)
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCompareView construction/destruction
CCompareView::CCompareView() : phev_(NULL), addr_width_(0)
{
}

CCompareView::~CCompareView()
{
}

void CCompareView::OnInitialUpdate()
{
    calc_addr_width(GetDocument()->length() + phev_->display_.addrbase1);

    CScrView::SetMapMode(MM_TEXT);

	ASSERT(phev_ != NULL && phev_->pfont_ != NULL && phev_->pbrush_ != NULL);
    SetFont(phev_->pfont_);
    SetBrush(phev_->pbrush_);

    CScrView::OnInitialUpdate();

	phev_->recalc_display();
    if (theApp.large_cursor_)
        BlockCaret();
    else
        LineCaret();

    CaretMode();
	// xxx check what happens with empty compare file

    ValidateScroll(GetScroll());
} // OnInitialUpdate

void CCompareView::OnDraw(CDC* pDC)
{
    if (phev_ == NULL || phev_->pfont_ == NULL) return;

    pDC->SetBkMode(TRANSPARENT);

    const char *hex;
    if (theApp.hex_ucase_)
        hex = "0123456789ABCDEF?";
    else
        hex = "0123456789abcdef?";

    CRectAp doc_rect;                           // Display area relative to whole document
    CRect norm_rect;                            // Display area (norm. logical coords)

    // These are the first and last "virtual" addresses of the top and (one past) the end
    // of the addresses within norm_rect.  Note that these are not necessarilly the first
    // and last real addresses.  For example, if offset_ != 0 and at the top of file then
    // first_virt will be -ve.  Similarly the file may not even be as long as last_virt.
    FILE_ADDRESS first_virt, last_virt;
    FILE_ADDRESS first_line;                    // First line that needs displaying
    FILE_ADDRESS last_line;                     // One past last line to display

    FILE_ADDRESS first_addr = 0;                // First address to actually display
    FILE_ADDRESS last_addr = GetDocument()->length(); // One past last address actually displayed xxx

    FILE_ADDRESS line_inc;                      // 1 or -1 depending on draw dirn (up/down)
    CSize rect_inc;                             // How much to move norm_rect each time
	int offset = phev_->offset_;
    FILE_ADDRESS start_addr, end_addr;          // Address of current selection
    bool neg_x(false), neg_y(false);            // Does map mode have -ve to left or down
    bool has_focus;                             // Does this window have focus?
    int bitspixel = pDC->GetDeviceCaps(BITSPIXEL);
    if (bitspixel > 24) bitspixel = 24;         // 32 is really only 24 bits of colour
    long num_colours = 1L << (bitspixel*pDC->GetDeviceCaps(PLANES));

    int line_height, char_width, char_width_w;  // Text sizes

	ASSERT(phev_->rowsize_ > 0 && phev_->rowsize_ <= CHexEditView::max_buf);
    ASSERT(phev_->group_by_ > 0);

	if (pDC->IsPrinting())
    {
		ASSERT(0); // TBD xxx
	}
    else
    {
        has_focus = (GetFocus() == this);
        HideCaret();
        neg_x = negx(); neg_y = negy();

        // Get display rect in logical units but with origin at top left of display area in window
        CRect rct;
        GetDisplayRect(&rct);
        doc_rect = ConvertFromDP(rct);

        // Display = client rectangle translated to posn in document
//        norm_rect = doc_rect - GetScroll();
        norm_rect.left  = doc_rect.left  - GetScroll().x + phev_->bdr_left_;
        norm_rect.right = doc_rect.right - GetScroll().x + phev_->bdr_left_;
        norm_rect.top    = int(doc_rect.top    - GetScroll().y) + phev_->bdr_top_;
        norm_rect.bottom = int(doc_rect.bottom - GetScroll().y) + phev_->bdr_top_;

        // Get the current selection so that we can display it in reverse video
        //GetSelAddr(start_addr, end_addr);
		start_addr = end_addr = 0;  // TBD xxx

        line_height = phev_->line_height_;
        char_width = phev_->text_width_;
        char_width_w = phev_->text_width_w_;
    }

    // Get range of addresses that are visible the in window (overridden for printing below)
    first_virt = (doc_rect.top/line_height) * phev_->rowsize_ - offset;
    last_virt  = (doc_rect.bottom/line_height + 1) * phev_->rowsize_ - offset;

    // Work out which lines could possibly be in the display area
    if (pDC->IsPrinting())
    {
		// TBD xxx
    }
    else if (ScrollUp())
    {
        // Draw from bottom of window up since we're scrolling up (looks better)
        first_line = doc_rect.bottom/line_height;
        last_line = doc_rect.top/line_height - 1;
        line_inc = -1L;
        rect_inc = CSize(0, -line_height);

        /* Work out where to display the 1st line */
        norm_rect.top -= int(doc_rect.top - first_line*line_height);
        norm_rect.bottom = norm_rect.top + line_height;
        norm_rect.left -= doc_rect.left;
    }
    else
    {
        // Draw from top of window down
        first_line = doc_rect.top/line_height;
        last_line = doc_rect.bottom/line_height + 1;
        line_inc = 1L;
        rect_inc = CSize(0, line_height);

        /* Work out where to display the 1st line */
        norm_rect.top -= int(doc_rect.top - first_line*line_height);
        norm_rect.bottom = norm_rect.top + line_height;
        norm_rect.left -= doc_rect.left;
    }

    if (first_addr < first_virt) first_addr = first_virt;
    if (last_addr > last_virt) last_addr = last_virt;

    // These are for drawing things on the screen
	CPoint pt;   // moved this here to avoid a spurious compiler error C2362
    CPen pen1(PS_SOLID, 0, same_hue(phev_->sector_col_, 100, 30));    // dark sector_col_
    CPen pen2(PS_SOLID, 0, same_hue(phev_->addr_bg_col_, 100, 30));   // dark addr_bg_col_
    CPen *psaved_pen;
    CBrush brush(phev_->sector_col_);

    if (phev_->display_.borders)
        psaved_pen = pDC->SelectObject(&pen1);
    else
        psaved_pen = pDC->SelectObject(&pen2);

    // Column related screen stuff (ruler, vertical lines etc)
    if (!pDC->IsPrinting())
    {
        ASSERT(!neg_y && !neg_x);       // This should be true when drawing on screen (uses MM_TEXT)
        // Vert. line between address and hex areas
		pt.y = phev_->bdr_top_ - 2;
		pt.x = addr_width_*char_width - char_width - doc_rect.left + phev_->bdr_left_;
		pDC->MoveTo(pt);
		pt.y = 30000;
		pDC->LineTo(pt);
        if (!phev_->display_.vert_display && phev_->display_.hex_area)
        {
			// Vert line to right of hex area
            pt.y = phev_->bdr_top_ - 2;
            pt.x = char_pos(0, char_width) - char_width_w/2 - doc_rect.left + phev_->bdr_left_;
            pDC->MoveTo(pt);
            pt.y = 30000;
            pDC->LineTo(pt);
        }
        if (phev_->display_.vert_display || phev_->display_.char_area)
        {
			// Vert line to right of char area
            pt.y = phev_->bdr_top_ - 2;
            pt.x = char_pos(phev_->rowsize_ - 1, char_width, char_width_w) + 
                   (3*char_width_w)/2 - doc_rect.left + phev_->bdr_left_;
            pDC->MoveTo(pt);
            pt.y = 30000;
            pDC->LineTo(pt);
        }
		if (theApp.ruler_)
		{
			int horz = phev_->bdr_left_ - GetScroll().x;       // Horiz. offset to window posn of left side

			ASSERT(phev_->bdr_top_ > 0);
			// Draw horiz line under ruler
			pt.y = phev_->bdr_top_ - 3;
			pt.x = addr_width_*char_width - char_width - doc_rect.left + phev_->bdr_left_;
			pDC->MoveTo(pt);
			pt.x = 30000;
			pDC->LineTo(pt);

            // Draw ticks using hex offsets for major ticks (if using hex addresses) or
            // decimal offsets (if using decimal addresses/line numbers and/or hex addresses)
            int major = 1;
			if (phev_->display_.decimal_addr || phev_->display_.line_nums)
                major = theApp.ruler_dec_ticks_;        // decimal ruler or both hex and decimal
            else
                major = theApp.ruler_hex_ticks_;        // only showing hex ruler
            // Hex area ticks
			if (!phev_->display_.vert_display && phev_->display_.hex_area)
				for (int column = 1; column < phev_->rowsize_; ++column)
                {
                    if ((!phev_->display_.decimal_addr && !phev_->display_.line_nums && theApp.ruler_hex_nums_ > 1 && column%theApp.ruler_hex_nums_ == 0) ||
                        ((phev_->display_.decimal_addr || phev_->display_.line_nums) && theApp.ruler_dec_nums_ > 1 && column%theApp.ruler_dec_nums_ == 0) )
                        continue;       // skip when displaying a number at this posn
        			pt.y = phev_->bdr_top_ - 4;
					pt.x = hex_pos(column) - char_width/2 + horz;
                    if (column%phev_->group_by_ == 0)
                        pt.x -= char_width/2;
        			pDC->MoveTo(pt);
                    pt.y -= (column%major) ? 3 : 7;
			        pDC->LineTo(pt);
                }
            // Char area or stacked display ticks
    		if (phev_->display_.vert_display || phev_->display_.char_area)
				for (int column = 0; column <= phev_->rowsize_; ++column)
                {
                    if ((!phev_->display_.decimal_addr && !phev_->display_.line_nums && theApp.ruler_hex_nums_ > 1 && column%theApp.ruler_hex_nums_ == 0) ||
                        ((phev_->display_.decimal_addr || phev_->display_.line_nums) && theApp.ruler_dec_nums_ > 1 && column%theApp.ruler_dec_nums_ == 0) )
                        continue;       // skip when displaying a number at this posn
        			pt.y = phev_->bdr_top_ - 4;
					pt.x = char_pos(column) + horz;
                    if (phev_->display_.vert_display && column%phev_->group_by_ == 0)
                        if (column == phev_->rowsize_)    // skip last one
                            break;
                        else
                            pt.x -= char_width/2;
        			pDC->MoveTo(pt);
                    pt.y -= (column%major) ? 2 : 5;
			        pDC->LineTo(pt);
                }

			// Draw numbers in the ruler area
            // Note that if we are displaying hex and decimal addresses we show 2 rows
            //      - hex offsets at top (then after moving vert down) decimal offsets
			int vert = 0;                       // Screen y pixel to the row of nos at

			// Get display rect for clipping at right and left
			CRect cli;
			GetDisplayRect(&cli);

			// Show hex offsets in the top border (ruler)
			if (phev_->display_.hex_addr)
			{
                bool between = theApp.ruler_hex_nums_ > 1;      // Only display numbers above cols if displaying for every column

                // Do hex numbers in ruler
                CRect rect(-1, vert, -1, vert + phev_->text_height_ + 1);
				CString ss;
				pDC->SetTextColor(GetHexAddrCol());   // Colour of hex addresses

                // Show hex offsets above hex area
				if (!phev_->display_.vert_display && phev_->display_.hex_area)
                    for (int column = 0; column < phev_->rowsize_; ++column)
					{
                        if (between && phev_->display_.addrbase1 && column == 0)
                            continue;           // usee doesn't like seeing zero
                        if (column%theApp.ruler_hex_nums_ != 0)
                            continue;
                        rect.left = hex_pos(column) + horz;
                        if (between)
                        {
                            rect.left -= char_width;
                            if (column > 0 && column%phev_->group_by_ == 0)
                                rect.left -= char_width/2;
                        }
                        if (rect.left < cli.left)
                            continue;
                        if (rect.left > cli.right)
                            break;
						rect.right = rect.left + phev_->text_width_ + phev_->text_width_;
                        if (!between)
                        {
                            // Draw 2 digit number above every column
                            ss.Format(theApp.hex_ucase_ ? "%02X" : "%02x", (column + phev_->display_.addrbase1)%256);
						    pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else if (column%16 == 0 && theApp.ruler_hex_nums_ > 3)
                        {
                            // Draw 2 digit numbers to mark end of 16 columns
                            rect.left -= (char_width+1)/2;
                            ss.Format(theApp.hex_ucase_ ? "%02X" : "%02x", column%256);
						    pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else
                        {
                            // Draw single digit number in between columns
                            ss.Format(theApp.hex_ucase_ ? "%1X" : "%1x", column%16);
						    pDC->DrawText(ss, &rect, DT_BOTTOM | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
					}
                // Show hex offsets above char area or stacked display
				if (phev_->display_.vert_display || phev_->display_.char_area)
                    for (int column = 0; column < phev_->rowsize_; ++column)
					{
                        if (between && phev_->display_.addrbase1 && column == 0)
                            continue;           // user doesn't like seeing zero
                        if (column%theApp.ruler_hex_nums_ != 0)
                            continue;
						rect.left = char_pos(column) + horz;
                        if (between)
                        {
                            if (phev_->display_.vert_display && column > 0 && column%phev_->group_by_ == 0)
                                rect.left -= char_width;
                            else
                                rect.left -= char_width/2;
                        }
						if (rect.left < cli.left)
                            continue;
                        if (rect.left > cli.right)
                            break;
						rect.right = rect.left + phev_->text_width_ + phev_->text_width_;

                        if (!between)
                        {
						    ss.Format(theApp.hex_ucase_ ? "%1X" : "%1x", (column + phev_->display_.addrbase1)%16);
						    pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else if (column%16 == 0 && theApp.ruler_hex_nums_ > 3)
                        {
                            rect.left -= (char_width+1)/2;
						    ss.Format(theApp.hex_ucase_ ? "%02X" : "%02x", column%256);
						    pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else
                        {
						    ss.Format(theApp.hex_ucase_ ? "%1X" : "%1x", column%16);
						    pDC->DrawText(ss, &rect, DT_BOTTOM | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
					}
				vert += phev_->text_height_;  // Move down for anything to be drawn underneath
			}
            // Show decimal offsets in the ruler
			if (phev_->display_.decimal_addr || phev_->display_.line_nums)
			{
                bool between = theApp.ruler_dec_nums_ > 1;      // Only display numbers above cols if displaying for every column

				CRect rect(-1, vert, -1, vert + phev_->text_height_ + 1);
				CString ss;
				pDC->SetTextColor(GetDecAddrCol());   // Colour of dec addresses

                // Decimal offsets above hex area
				if (!phev_->display_.vert_display && phev_->display_.hex_area)
					for (int column = 0; column < phev_->rowsize_; ++column)
					{
                        if (between && phev_->display_.addrbase1 && column == 0)
                            continue;           // user doesn't like seeing zero
                        if (column%theApp.ruler_dec_nums_ != 0)
                            continue;
						rect.left  = hex_pos(column) + horz;
                        if (between)
                        {
                            rect.left -= char_width;
                            if (column > 0 && column%phev_->group_by_ == 0)
                                rect.left -= char_width/2;
                        }
						if (rect.left < cli.left)
                            continue;
                        if (rect.left > cli.right)
                            break;
						rect.right = rect.left + phev_->text_width_ + phev_->text_width_;
                        if (!between)
                        {
						    ss.Format("%02d", (column + phev_->display_.addrbase1)%100);
    						pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else if (column%10 == 0 && theApp.ruler_dec_nums_ > 4)
                        {
                            rect.left -= (char_width+1)/2;
						    ss.Format("%02d", column%100);
    						pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else
                        {
						    ss.Format("%1d", column%10);
						    pDC->DrawText(ss, &rect, DT_BOTTOM | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
					}
                // Decimal offsets above char area or stacked display
				if (phev_->display_.vert_display || phev_->display_.char_area)
					for (int column = 0; column < phev_->rowsize_; ++column)
					{
                        if (between && phev_->display_.addrbase1 && column == 0)
                            continue;           // user doesn't like seeing zero
                        if (column%theApp.ruler_dec_nums_ != 0)
                            continue;
						rect.left = char_pos(column) + horz;
                        if (between)
                        {
                            if (phev_->display_.vert_display && column > 0 && column%phev_->group_by_ == 0)
                                rect.left -= char_width;
                            else
                                rect.left -= char_width/2;
                        }
						if (rect.left < cli.left)
                            continue;
                        if (rect.left > cli.right)
                            break;
						rect.right = rect.left + phev_->text_width_ + phev_->text_width_;

                        if (!between)
                        {
						    ss.Format("%1d", (column + phev_->display_.addrbase1)%10);
						    pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else if (column%10 == 0 && theApp.ruler_dec_nums_ > 4)
                        {
                            // If displaying nums every 5 or 10 then display 2 digits fo tens column
                            rect.left -= (char_width+1)/2;
						    ss.Format("%02d", column%100);
						    pDC->DrawText(ss, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                        else
                        {
                            // Display single dit between columns
						    ss.Format("%1d", column%10);
						    pDC->DrawText(ss, &rect, DT_BOTTOM | DT_LEFT | DT_NOPREFIX | DT_SINGLELINE);
                        }
                    }
				vert += phev_->text_height_;   // Move down for anything to be drawn underneath (currently nothing)
			}
		} // end ruler drawing
    }

	// Mask out the ruler so we don't get top of topmost line drawn into it.
	// Doing it this way allows the address of the top line to be drawn
	// higher (ie into ruler area) without being clipped.
	// Note that we currently only use bdr_top_ (for the ruler) but if
	// other borders are used similarly we would need to clip them too.
	// Note: This needs to be done after drawing the ruler.
#ifndef TEST_CLIPPING
    if (!pDC->IsPrinting())
	{
		CRect rct;
		GetDisplayRect(&rct);
		rct.bottom = rct.top;
		rct.top = 0;
		rct.left = (addr_width_ - 1)*char_width + norm_rect.left + phev_->bdr_left_;
		pDC->ExcludeClipRect(&rct);
	}
#endif
    pDC->SelectObject(psaved_pen);      // restore pen after drawing borders etc

#if 0 // TBD xxx convert to diff-tracking
	// Draw deletion marks always from top (shouldn't be too visible)
    if (!display_.hide_delete)
    {
        COLORREF prev_col = pDC->SetTextColor(bg_col_);

        std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *ppr =
            GetDocument()->Deletions();
        std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> >::const_iterator pp;
        for (pp = ppr->begin(); pp != ppr->end(); ++pp)
        {
            // Check if its before or after the display area
            if (pp->first < first_virt)
                continue;
            else if (pp->first > last_virt)
                break;

            CRect draw_rect;

            draw_rect.top = int(((pp->first + offset_)/rowsize_) * line_height - 
                                doc_rect.top + bdr_top_);
            draw_rect.bottom = draw_rect.top + line_height;
            if (neg_y)
            {
                draw_rect.top = -draw_rect.top;
                draw_rect.bottom = -draw_rect.bottom;
            }

            if (!display_.vert_display && display_.hex_area)
            {
                draw_rect.left = hex_pos(int((pp->first + offset_)%rowsize_), char_width) - 
                                    char_width - doc_rect.left + bdr_left_;
                draw_rect.right = draw_rect.left + char_width;
                if (neg_x)
                {
                    draw_rect.left = -draw_rect.left;
                    draw_rect.right = -draw_rect.right;
                }
                pDC->FillSolidRect(&draw_rect, trk_col_);
                char cc = (pp->second > 9 || !display_.delete_count) ? '*' : '0' + char(pp->second);
                pDC->DrawText(&cc, 1, &draw_rect, DT_CENTER | DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
            }
            if (display_.vert_display || display_.char_area)
            {
                draw_rect.left = char_pos(int((pp->first + offset_)%rowsize_), char_width, char_width_w) - 
                                    doc_rect.left + bdr_left_ - 2;
                draw_rect.right = draw_rect.left + char_width_w/5+1;
                if (neg_x)
                {
                    draw_rect.left = -draw_rect.left;
                    draw_rect.right = -draw_rect.right;
                }
                pDC->FillSolidRect(&draw_rect, trk_col_);
            }
        }
        pDC->SetTextColor(prev_col);   // restore text colour
    }

    // Now draw other change tracking stuff top down OR bottom up depending on ScrollUp()
    if (!pDC->IsPrinting() && ScrollUp())
    {
        // Draw change tracking from bottom up
        if (!display_.hide_replace)
        {
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *ppr =
                GetDocument()->Replacements();
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> >::reverse_iterator pp;
            for (pp = ppr->rbegin(); pp != ppr->rend(); ++pp)
                if (pp->first < last_virt)
                    break;
            for ( ; pp != ppr->rend(); ++pp)
            {
                if (pp->first + pp->second <= first_virt)
                    break;
                draw_bg(pDC, doc_rect, neg_x, neg_y,
                        line_height, char_width, char_width_w, trk_col_,
                        max(pp->first, first_addr), 
                        min(pp->first + pp->second, last_addr), (pDC->IsPrinting() ? print_text_height_ : text_height_)/8);
            }
        }
        if (!display_.hide_insert)
        {
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *ppr =
                GetDocument()->Insertions();
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> >::reverse_iterator pp;
            for (pp = ppr->rbegin(); pp != ppr->rend(); ++pp)
                if (pp->first < last_virt)
                    break;
            for ( ; pp != ppr->rend(); ++pp)
            {
                if (pp->first +pp->second <= first_virt)
                    break;
                draw_bg(pDC, doc_rect, neg_x, neg_y,
                        line_height, char_width, char_width_w, trk_bg_col_,
                        max(pp->first, first_addr), 
                        min(pp->first + pp->second, last_addr));
            }
        }
    }
    else
    {
        // Draw change tracking from top down
        if (!display_.hide_replace)
        {
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *ppr =
                GetDocument()->Replacements();
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> >::const_iterator pp;
            for (pp = ppr->begin(); pp != ppr->end(); ++pp)
                if (pp->first + pp->second > first_virt)
                    break;
            for ( ; pp != ppr->end(); ++pp)
            {
                if (pp->first > last_virt)
                    break;
                draw_bg(pDC, doc_rect, neg_x, neg_y,
                        line_height, char_width, char_width_w, trk_col_,
                        max(pp->first, first_addr), 
                        min(pp->first + pp->second, last_addr), (pDC->IsPrinting() ? print_text_height_ : text_height_)/8);
            }
        }
        if (!display_.hide_insert)
        {
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> > *ppr =
                GetDocument()->Insertions();
            std::vector<pair<FILE_ADDRESS, FILE_ADDRESS> >::const_iterator pp;
            for (pp = ppr->begin(); pp != ppr->end(); ++pp)
                if (pp->first + pp->second > first_virt)
                    break;
            for ( ; pp != ppr->end(); ++pp)
            {
                if (pp->first > last_virt)
                    break;
                draw_bg(pDC, doc_rect, neg_x, neg_y,
                        line_height, char_width, char_width_w, trk_bg_col_,
                        max(pp->first, first_addr), 
                        min(pp->first + pp->second, last_addr));
            }
        }
    }
#endif

	unsigned char buf[CHexEditView::max_buf];  // Holds bytes for current line being displayed
    size_t last_col = 0;                     // Number of bytes in buf to display

    // Move declarations outside loop (faster?)
    CString ss(' ', 24);                     // Temp string for formatting
    CRect tt;                                // Temp rect
    CRect addr_rect;                         // Where address is drawn

    // This was added for vert_display
    int vert_offset = 0;
    if (phev_->display_.vert_display)
    {
        if (pDC->IsPrinting())
            vert_offset = phev_->print_text_height_;
        else
            vert_offset = phev_->text_height_;
        if (neg_y)
            vert_offset = - vert_offset;
    }

	// THIS IS WHERE THE ACTUAL LINES ARE DRAWN
    // Note: we use != (line != last_line) since we may be drawing from bottom or top
    for (FILE_ADDRESS line = first_line; line != last_line;
                            line += line_inc, norm_rect += rect_inc)
    {
        // Work out where to display line in logical coords (correct sign)
        tt = norm_rect;
        if (neg_x)
        {
            tt.left = -tt.left;
            tt.right = -tt.right;
        }
        if (neg_y)
        {
            tt.top = -tt.top;
            tt.bottom = -tt.bottom;
        }

        // No display needed if outside display area or past end of doc
        // Note: we don't break when past end since we may be drawing from bottom
        if (!pDC->RectVisible(&tt) || line*phev_->rowsize_ - offset > GetDocument()->length())
            continue;

        // Get the bytes to display
        size_t ii;                      // Column of first byte

        if (line*phev_->rowsize_ - offset < first_addr)
        {
            last_col = GetDocument()->GetData(buf + offset, phev_->rowsize_ - offset, line*phev_->rowsize_) +
                        offset;
            ii = size_t(first_addr - (line*phev_->rowsize_ - offset));
            ASSERT(int(ii) < phev_->rowsize_);
        }
        else
        {
            last_col = GetDocument()->GetData(buf, phev_->rowsize_, line*phev_->rowsize_ - offset);
            ii = 0;
        }

        if (line*phev_->rowsize_ - offset + last_col - last_addr >= phev_->rowsize_)
            last_col = 0;
        else if (line*phev_->rowsize_ - offset + last_col > last_addr)
            last_col = size_t(last_addr - (line*phev_->rowsize_ - offset));

        // Draw address if ...
        if ((addr_width_ - 1)*char_width + tt.left > 0 &&    // not off to the left
			(tt.top + phev_->text_height_/4 >= phev_->bdr_top_ || pDC->IsPrinting()))   // and does not encroach into ruler
        {
            addr_rect = tt;            // tt with right margin where addresses end
            addr_rect.right = addr_rect.left + addr_width_*char_width - char_width - 1;
			if (pDC->IsPrinting())
				if (neg_y)
					addr_rect.bottom = addr_rect.top - phev_->print_text_height_;
				else
					addr_rect.bottom = addr_rect.top + phev_->print_text_height_;
			else
				if (neg_y)
					addr_rect.bottom = addr_rect.top - phev_->text_height_;
				else
					addr_rect.bottom = addr_rect.top + phev_->text_height_;

            if (hex_width_ > 0)
            {
				int ww = hex_width_ + 1;
                char *addr_buf = ss.GetBuffer(24);             // reserve space for 64 bit address
                sprintf(addr_buf,
                    theApp.hex_ucase_ ? "%0*I64X:" : "%0*I64x:",
                        hex_width_,
                        (line*phev_->rowsize_ - offset > first_addr ? line*phev_->rowsize_ - offset : first_addr) + phev_->display_.addrbase1);
                ss.ReleaseBuffer(-1);
                if (theApp.nice_addr_)
				{
                    AddSpaces(ss);
					ww += (hex_width_-1)/4;
				}
                pDC->SetTextColor(GetHexAddrCol());   // Colour of hex addresses
                addr_rect.right = addr_rect.left + ww*char_width;
                pDC->DrawText(ss, &addr_rect, DT_TOP | DT_NOPREFIX | DT_SINGLELINE);
                addr_rect.left = addr_rect.right;
            }

            if (dec_width_ > 0)
            {
				int ww = dec_width_ + 1;
                char *addr_buf = ss.GetBuffer(24);             // reserve space for 64 bit address
                sprintf(addr_buf,
                        "%*I64d:",
                        dec_width_,
                        (line*phev_->rowsize_ - offset > first_addr ? line*phev_->rowsize_ - offset : first_addr) + phev_->display_.addrbase1);
                ss.ReleaseBuffer(-1);
                if (theApp.nice_addr_)
				{
                    AddCommas(ss);
					ww += (dec_width_-1)/3;
				}
                pDC->SetTextColor(GetDecAddrCol());   // Colour of dec addresses
                addr_rect.right = addr_rect.left + ww*char_width;
                pDC->DrawText(ss, &addr_rect, DT_TOP | DT_RIGHT | DT_NOPREFIX | DT_SINGLELINE);
                addr_rect.left = addr_rect.right;
            }

            if (num_width_ > 0)
            {
				int ww = num_width_ + 1;
                char *addr_buf = ss.GetBuffer(24);             // reserve space for 64 bit address
                sprintf(addr_buf,
                        "%*I64d:",
                        num_width_,
                        line + phev_->display_.addrbase1);
                ss.ReleaseBuffer(-1);
                if (theApp.nice_addr_)
				{
                    AddCommas(ss);
					ww += (num_width_-1)/3;
				}
                pDC->SetTextColor(GetDecAddrCol());   // Colour of dec addresses
                addr_rect.right = addr_rect.left + ww*char_width;
                pDC->DrawText(ss, &addr_rect, DT_TOP | DT_RIGHT | DT_NOPREFIX | DT_SINGLELINE);
                addr_rect.left = addr_rect.right;
            }
        }

        // Keep track of the current colour so we only set it when it changes
        COLORREF current_colour = phev_->kala[buf[ii]];
        pDC->SetTextColor(current_colour);
        if (phev_->display_.vert_display || phev_->display_.hex_area)
        {
            int posx = tt.left + hex_pos(0, char_width);                 // Horiz pos of 1st hex column

            // Display each byte as hex (and char if nec.)
            for (size_t jj = ii ; jj < last_col; ++jj)
            {
                if (phev_->display_.vert_display)
                {
                    if (posx + int(jj + 1 + jj/phev_->group_by_)*char_width_w < 0)
                        continue;
                    else if (posx + int(jj + jj/phev_->group_by_)*char_width_w >= tt.right)
                        break;
                }
                else
                {
                    if (posx + int((jj+1)*3 + jj/phev_->group_by_)*char_width < 0)
                        continue;
                    else if (posx + int(jj*3 + jj/phev_->group_by_)*char_width >= tt.right)
                        break;
                }

                if (current_colour != phev_->kala[buf[jj]])
                {
                    current_colour = phev_->kala[buf[jj]];
                    pDC->SetTextColor(current_colour);
                }

                if (phev_->display_.vert_display)
                {
                    // Now display the character in the top row
                    if (phev_->display_.char_set != CHARSET_EBCDIC)
                    {
                        if ((buf[jj] >= 32 && buf[jj] < 127) ||
                            (phev_->display_.char_set != CHARSET_ASCII && buf[jj] >= phev_->first_char_ && buf[jj] <= phev_->last_char_) )
                        {
                            // Display normal char or graphic char if in font
                            pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top, (char *)&buf[jj], 1);
                        }
                        else if (phev_->display_.control == 0 || buf[jj] >= 32)
                        {
                            // Display control char and other chars as red '.'
                            pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top, ".", 1);
                        }
                        else if (phev_->display_.control == 1)
                        {
                            // Display control chars as red uppercase equiv.
                            char cc = buf[jj] + 0x40;
                            pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top, &cc, 1);
                        }
                        else if (phev_->display_.control == 2)
                        {
                            // Display control chars as C escape code (in red)
                            const char *check = "\a\b\f\n\r\t\v\0";
                            const char *display = "abfnrtv0";
                            const char *pp;
                            if (/*buf[jj] != '\0' && */(pp = strchr(check, buf[jj])) != NULL)
                                pp = display + (pp-check);
                            else
                                pp = ".";
                            pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top, pp, 1);
                        }
                    }
                    else
                    {
                        // Display EBCDIC (or red dot if not valid EBCDIC char)
                        if (e2a_tab[buf[jj]] == '\0')
                        {
                            pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top, ".", 1);
                        }
                        else
                        {
                            pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top, (char *)&e2a_tab[buf[jj]], 1);
                        }
                    }

                    // Display the hex digits below that, one below the other
                    pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top + vert_offset,   &hex[(buf[jj]>>4)&0xF], 1);
                    pDC->TextOut(posx + (jj + jj/phev_->group_by_)*char_width_w, tt.top + vert_offset*2, &hex[buf[jj]&0xF], 1);
                }
                else
                {
                    char hh[2];

                    // Create hex digits and display them
                    hh[0] = hex[(buf[jj]>>4)&0xF];
                    hh[1] = hex[buf[jj]&0xF];

                    // This actually displays the bytes (in hex)!
                    // Note: removed calcs that were previously encapsulated in hex_pos
                    pDC->TextOut(posx + (jj*3 + jj/phev_->group_by_)*char_width, tt.top, hh, 2);
                }
            }
        }

        if (!phev_->display_.vert_display && phev_->display_.char_area)
        {
            // Keep track of the current colour so we only set it when it changes
            int posc = tt.left + char_pos(0, char_width, char_width_w);  // Horiz pos of 1st char column

            for (size_t kk = ii ; kk < last_col; ++kk)
            {
                if (posc + int(kk+1)*char_width_w < 0)
                    continue;
                else if (posc + int(kk)*char_width_w >= tt.right)
                    break;

                if (current_colour != phev_->kala[buf[kk]])
                {
                    current_colour = phev_->kala[buf[kk]];
                    pDC->SetTextColor(current_colour);
                }

                // Display byte in char display area (as ASCII, EBCDIC etc)
                if (phev_->display_.char_set != CHARSET_EBCDIC)
                {
                    if ((buf[kk] >= 32 && buf[kk] < 127) ||
                        (phev_->display_.char_set != CHARSET_ASCII && buf[kk] >= phev_->first_char_ && buf[kk] <= phev_->last_char_) )
                    {
                        // Display normal char or graphic char if in font
                        pDC->TextOut(posc + kk*char_width_w, tt.top, (char *)&buf[kk], 1);
                    }
                    else if (phev_->display_.control == 0 || buf[kk] > 31)
                    {
                        // Display control char and other chars as red '.'
                        pDC->TextOut(posc + kk*char_width_w, tt.top, ".", 1);
                    }
                    else if (phev_->display_.control == 1)
                    {
                        // Display control chars as red uppercase equiv.
                        char cc = buf[kk] + 0x40;
                        pDC->TextOut(posc + kk*char_width_w, tt.top, &cc, 1);
                    }
                    else if (phev_->display_.control == 2)
                    {
                        // Display control chars as C escape code (in red)
                        const char *check = "\a\b\f\n\r\t\v\0";
                        const char *display = "abfnrtv0";
                        const char *pp;
                        if (/*buf[kk] != '\0' && */(pp = strchr(check, buf[kk])) != NULL)
                            pp = display + (pp-check);
                        else
                            pp = ".";
                        pDC->TextOut(posc + kk*char_width_w, tt.top, pp, 1);
                    }
                }
                else
                {
                    // Display EBCDIC (or red dot if not valid EBCDIC char)
                    if (e2a_tab[buf[kk]] == '\0')
                    {
                        pDC->TextOut(posc + kk*char_width_w, tt.top, ".", 1);
                    }
                    else
                    {
                        pDC->TextOut(posc + kk*char_width_w, tt.top, (char *)&e2a_tab[buf[kk]], 1);
                    }
                }
            }
        }

        // If any part of the line is within the current selection
        if (!pDC->IsPrinting() && start_addr < end_addr &&
            end_addr > line*phev_->rowsize_ - offset && start_addr < (line+1)*phev_->rowsize_ - offset)
        {
            FILE_ADDRESS start = max(start_addr, line*phev_->rowsize_ - offset);
            FILE_ADDRESS   end = min(end_addr, (line+1)*phev_->rowsize_ - offset);
//            ASSERT(end > start);

            ASSERT(phev_->display_.hex_area || phev_->display_.char_area);
            if (!phev_->display_.vert_display && phev_->display_.hex_area)
            {
                CRect rev(norm_rect);
                rev.right = rev.left + hex_pos(int(end - (line*phev_->rowsize_ - offset) - 1)) + 2*phev_->text_width_;
                rev.left += hex_pos(int(start - (line*phev_->rowsize_ - offset)));
                if (neg_x)
                {
                    rev.left = -rev.left;
                    rev.right = -rev.right;
                }
                if (neg_y)
                {
                    rev.top = -rev.top;
                    rev.bottom = -rev.bottom;
                }
                if (has_focus && !phev_->display_.edit_char || num_colours <= 256)
                    pDC->InvertRect(&rev);  // Full contrast reverse video only if in editing in hex area
                else
                    pDC->PatBlt(rev.left, rev.top,
                                rev.right-rev.left, rev.bottom-rev.top,
                                PATINVERT);
            }

            if (phev_->display_.vert_display || phev_->display_.char_area)
            {
                // Draw char selection in inverse
                CRect rev(norm_rect);
                rev.right = rev.left + char_pos(int(end - (line*phev_->rowsize_ - offset) - 1)) + phev_->text_width_w_;
                rev.left += char_pos(int(start - (line*phev_->rowsize_ - offset)));
                if (neg_x)
                {
                    rev.left = -rev.left;
                    rev.right = -rev.right;
                }
                if (neg_y)
                {
                    rev.top = -rev.top;
                    rev.bottom = -rev.bottom;
                }
                if (num_colours <= 256 || has_focus && (phev_->display_.vert_display || phev_->display_.edit_char))
                    pDC->InvertRect(&rev);
                else
                    pDC->PatBlt(rev.left, rev.top,
                                rev.right-rev.left, rev.bottom-rev.top,
                                PATINVERT);
            }
        }
        else if (theApp.show_other_ && /* has_focus && */
                 !phev_->display_.vert_display &&
                 phev_->display_.char_area && phev_->display_.hex_area &&  // we can only display in the other area if both exist
                 !pDC->IsPrinting() &&
                 start_addr == end_addr &&
                 start_addr >= line*phev_->rowsize_ - offset && 
                 start_addr < (line+1)*phev_->rowsize_ - offset)
        {
            // Draw "shadow" cursor in the other area
            FILE_ADDRESS start = max(start_addr, line*phev_->rowsize_ - offset);
            FILE_ADDRESS   end = min(end_addr, (line+1)*phev_->rowsize_ - offset);

            CRect rev(norm_rect);
            if (phev_->display_.edit_char)
            {
                ASSERT(phev_->display_.char_area);
                rev.right = rev.left + hex_pos(int(end - (line*phev_->rowsize_ - offset))) + 2*phev_->text_width_;
                rev.left += hex_pos(int(start - (line*phev_->rowsize_ - offset)));
            }
            else
            {
                ASSERT(phev_->display_.hex_area);
                rev.right = rev.left + char_pos(int(end - (line*phev_->rowsize_ - offset))) + phev_->text_width_w_;
                rev.left += char_pos(int(start - (line*phev_->rowsize_ - offset)));
            }
            if (neg_x)
            {
                rev.left = -rev.left;
                rev.right = -rev.right;
            }
            if (neg_y)
            {
                rev.top = -rev.top;
                rev.bottom = -rev.bottom;
            }
            pDC->PatBlt(rev.left, rev.top, rev.right-rev.left, rev.bottom-rev.top, PATINVERT);
        }
    } // for each display (text) line

    if (!pDC->IsPrinting())
    {
        ShowCaret();
    }
} // OnDraw


// These are like the CHexEditView versions (pos_hex and pos_char)
// but are duplicated here as we have our own addr_width_ member.
int CCompareView::pos_hex(int pos, int inside) const
{
    int col = pos - addr_width_*phev_->text_width_;
    col -= (col/(phev_->text_width_*(phev_->group_by_*3+1)))*phev_->text_width_;
    col = col/(3*phev_->text_width_);

    // Make sure col is within valid range
    if (col < 0) col = 0;
    else if (inside == 1 && col >= phev_->rowsize_) col = phev_->rowsize_ - 1;
    else if (inside == 0 && col > phev_->rowsize_) col = phev_->rowsize_;

    return col;
}

int CCompareView::pos_char(int pos, int inside) const
{
    int col;
    if (phev_->display_.vert_display)
    {
        col = (pos - addr_width_*phev_->text_width_)/phev_->text_width_w_;
        col -= col/(phev_->group_by_+1);
    }
    else if (phev_->display_.hex_area)
        col = (pos - addr_width_*phev_->text_width_ -
               phev_->rowsize_*3*phev_->text_width_ -
			   ((phev_->rowsize_-1)/phev_->group_by_)*phev_->text_width_) / phev_->text_width_w_;
    else
        col = (pos - addr_width_*phev_->text_width_) / phev_->text_width_w_;

    // Make sure col is within valid range
    if (col < 0) col = 0;
    else if (inside == 1 && col >= phev_->rowsize_) col = phev_->rowsize_ - 1;
    else if (inside == 0 && col > phev_->rowsize_) col = phev_->rowsize_;

    return col;
}

// Relies on CHexEditView::recalc_display to do most of the work.
void CCompareView::recalc_display()
{
	if (phev_ == NULL) return;

	// Ensure ScrView scrolling matches global options
    if (GetScrollPastEnds() != theApp.scroll_past_ends_)
    {
        SetScrollPastEnds(theApp.scroll_past_ends_);
        SetScroll(GetScroll());
    }
    SetAutoscroll(theApp.autoscroll_accel_);

	// phev_->recalc_display();  // we now assume that the hex view is up to date

	// Make sure our borders are the same as hex view's
	bdr_top_ = phev_->bdr_top_;
	bdr_bottom_ = phev_->bdr_bottom_;
	bdr_left_ = phev_->bdr_left_;
	bdr_right_ = phev_->bdr_right_;

	// Calculate width of address area which may be different than hex view's address width
	// TBD xxx replace GetDocument()->length() with compare file length
    calc_addr_width(GetDocument()->length() + phev_->display_.addrbase1);

    if (phev_->display_.vert_display)
        SetTSize(CSizeAp(-1, ((GetDocument()->length() + phev_->offset_)/phev_->rowsize_ + 1)*3));  // 3 rows of text
    else
        SetTSize(CSizeAp(-1, (GetDocument()->length() + phev_->offset_)/phev_->rowsize_ + 1));

    // Make sure we know the width of the display area
    if (phev_->display_.vert_display || phev_->display_.char_area)
        SetSize(CSize(char_pos(phev_->rowsize_-1)+phev_->text_width_w_+phev_->text_width_w_/2+1, -1));
    else
    {
        ASSERT(phev_->display_.hex_area);
        SetSize(CSize(hex_pos(phev_->rowsize_-1)+2*phev_->text_width_+phev_->text_width_/2+1, -1));
    }
} /* recalc_display() */

void CCompareView::calc_addr_width(FILE_ADDRESS length)
{
    hex_width_ = phev_->display_.hex_addr ? SigDigits(length, 16) : 0;
    dec_width_ = phev_->display_.decimal_addr ? SigDigits(length) : 0;
	num_width_ = phev_->display_.line_nums ? SigDigits(length/phev_->rowsize_) : 0;

    addr_width_ = hex_width_ + dec_width_ + num_width_;

    // Allow for separators (spaces and commas)
    if (theApp.nice_addr_)
        addr_width_ += (hex_width_-1)/4 + (dec_width_-1)/3 + (num_width_-1)/3;

    // Also add 1 for the colon
    addr_width_ += hex_width_ > 0 ? 1 : 0;
    addr_width_ += dec_width_ > 0 ? 1 : 0;
    addr_width_ += num_width_ > 0 ? 1 : 0;
	++addr_width_;
}

FILE_ADDRESS CCompareView::pos2addr(CPointAp pos, BOOL inside /*= TRUE*/) const
{
    FILE_ADDRESS address;
    address = (pos.y/phev_->line_height_)*phev_->rowsize_ - phev_->offset_;
    if (phev_->display_.vert_display || phev_->display_.edit_char)
        address += pos_char(pos.x, inside);
    else
        address += pos_hex(pos.x + phev_->text_width_/2, inside);
    return address;
}


CPointAp CCompareView::addr2pos(FILE_ADDRESS address, int row /*=0*/) const
{
    ASSERT(row == 0 || (phev_->display_.vert_display && row < 3));
    address += phev_->offset_;
    if (phev_->display_.vert_display)
        return CPointAp(char_pos(int(address%phev_->rowsize_)), (address/phev_->rowsize_) * phev_->line_height_ + row * phev_->text_height_);
    else if (phev_->display_.edit_char)
        return CPointAp(char_pos(int(address%phev_->rowsize_)), address/phev_->rowsize_ * phev_->line_height_);
    else
        return CPointAp(hex_pos(int(address%phev_->rowsize_)), address/phev_->rowsize_ * phev_->line_height_);
}

/////////////////////////////////////////////////////////////////////////////
// Overrides

void CCompareView::ValidateCaret(CPointAp &pos, BOOL inside /*=true*/)
{
	ASSERT(phev_ != NULL);
    // Ensure pos is a valid caret position or move it to the closest such one
    FILE_ADDRESS address = pos2addr(pos, inside);
    if (address < 0)
        address = 0;
    else if (address > GetDocument()->length())
        address = GetDocument()->length();
    pos = addr2pos(address);
}

/////////////////////////////////////////////////////////////////////////////
// CCompareView message handlers

void CCompareView::OnSize(UINT nType, int cx, int cy) 
{
    if (phev_ != NULL && (cx != 0 || cy != 0))
        phev_->recalc_display();     // this may result in it being called twice when using a splitter (unfortunate)

    CScrView::OnSize(nType, cx, cy);
}

BOOL CCompareView::OnEraseBkgnd(CDC* pDC)
{
    if (phev_ == NULL || phev_->bg_col_ == -1)
		return CScrView::OnEraseBkgnd(pDC);

    CRect rct;
    GetClientRect(rct);

    // Fill background with bg_col_
    CBrush backBrush;
    backBrush.CreateSolidBrush(phev_->bg_col_);
    backBrush.UnrealizeObject();
    pDC->FillRect(rct, &backBrush);

    // Get rect for address area
    rct.right = addr_width_*phev_->text_width_ - GetScroll().x - phev_->text_width_ + phev_->bdr_left_;

    // If address area is visible and address background is different to normal background ...
    if (rct.right > rct.left && phev_->addr_bg_col_ != phev_->bg_col_)
    {
        // Draw address background too
        CBrush addrBrush;
        addrBrush.CreateSolidBrush(phev_->addr_bg_col_);
        addrBrush.UnrealizeObject();
        pDC->FillRect(rct, &addrBrush);
    }
    if (theApp.ruler_ && phev_->addr_bg_col_ != phev_->bg_col_)
    {
		// Ruler background
	    GetClientRect(rct);
		rct.bottom = phev_->bdr_top_ - 2;
        CBrush addrBrush;
        addrBrush.CreateSolidBrush(phev_->addr_bg_col_);
        addrBrush.UnrealizeObject();
        pDC->FillRect(rct, &addrBrush);
    }

    return TRUE;
}