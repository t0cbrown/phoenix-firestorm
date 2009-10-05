/** 
 * @file lltooltip.cpp
 * @brief LLToolTipMgr class implementation and related classes
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

// self include
#include "lltooltip.h"

// Library includes
#include "lltextbox.h"
#include "lliconctrl.h"
#include "llui.h"			// positionViewNearMouse()
#include "llwindow.h"

//
// Constants
//

//
// Local globals
//

LLToolTipView *gToolTipView = NULL;

//
// Member functions
//

LLToolTipView::Params::Params()
{
	mouse_opaque = false;
}

LLToolTipView::LLToolTipView(const LLToolTipView::Params& p)
:	LLView(p)
{
}

void LLToolTipView::draw()
{
	LLToolTipMgr::instance().updateToolTipVisibility();

	// do the usual thing
	LLView::draw();
}

BOOL LLToolTipView::handleHover(S32 x, S32 y, MASK mask)
{
	static S32 last_x = x;
	static S32 last_y = y;

	LLToolTipMgr& tooltip_mgr = LLToolTipMgr::instance();

	if (x != last_x && y != last_y)
	{
		// allow new tooltips because mouse moved
		tooltip_mgr.unblockToolTips();
	}

	last_x = x;
	last_y = y;
	return LLView::handleHover(x, y, mask);
}

BOOL LLToolTipView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	LLToolTipMgr::instance().blockToolTips();
	return LLView::handleMouseDown(x, y, mask);
}

BOOL LLToolTipView::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	LLToolTipMgr::instance().blockToolTips();
	return LLView::handleMiddleMouseDown(x, y, mask);
}

BOOL LLToolTipView::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	LLToolTipMgr::instance().blockToolTips();
	return LLView::handleRightMouseDown(x, y, mask);
}


BOOL LLToolTipView::handleScrollWheel( S32 x, S32 y, S32 clicks )
{
	LLToolTipMgr::instance().blockToolTips();
	return FALSE;
}

void LLToolTipView::onMouseLeave(S32 x, S32 y, MASK mask)
{
	LLToolTipMgr::instance().blockToolTips();
}


void LLToolTipView::drawStickyRect()
{
	gl_rect_2d(LLToolTipMgr::instance().getMouseNearRect(), LLColor4::white, false);
}
//
// LLToolTip
//


static LLDefaultChildRegistry::Register<LLToolTip> r("tool_tip");


LLToolTip::Params::Params()
:	max_width("max_width", 200),
	padding("padding", 4),
	pos("pos"),
	message("message"),
	delay_time("delay_time", LLUI::sSettingGroups["config"]->getF32( "ToolTipDelay" )),
	visible_time_over("visible_time_over", LLUI::sSettingGroups["config"]->getF32( "ToolTipVisibleTimeOver" )),
	visible_time_near("visible_time_near", LLUI::sSettingGroups["config"]->getF32( "ToolTipVisibleTimeNear" )),
	visible_time_far("visible_time_far", LLUI::sSettingGroups["config"]->getF32( "ToolTipVisibleTimeFar" )),
	sticky_rect("sticky_rect"),
	image("image")
{
	name = "tooltip";
	font = LLFontGL::getFontSansSerif();
	bg_opaque_color = LLUIColorTable::instance().getColor( "ToolTipBgColor" );
	background_visible = true;
}

LLToolTip::LLToolTip(const LLToolTip::Params& p)
:	LLPanel(p),
	mMaxWidth(p.max_width),
	mHasClickCallback(p.click_callback.isProvided()),
	mPadding(p.padding)
{
	LLTextBox::Params params;
	params.initial_value = "tip_text";
	params.name = params.initial_value().asString();
	// bake textbox padding into initial rect
	params.rect = LLRect (mPadding, mPadding + 1, mPadding + 1, mPadding);
	params.follows.flags = FOLLOWS_ALL;
	params.h_pad = 0;
	params.v_pad = 0;
	params.mouse_opaque = false;
	params.text_color = LLUIColorTable::instance().getColor( "ToolTipTextColor" );
	params.bg_visible = false;
	params.font = p.font;
	params.use_ellipses = true;
	mTextBox = LLUICtrlFactory::create<LLTextBox> (params);
	addChild(mTextBox);

	if (p.image.isProvided())
	{
		LLIconCtrl::Params icon_params;
		icon_params.name = "tooltip_icon";
		LLRect icon_rect;
		LLUIImage* imagep = p.image;
		const S32 TOOLTIP_ICON_SIZE = (imagep ? imagep->getWidth() : 16);
		icon_rect.setOriginAndSize(mPadding, mPadding, TOOLTIP_ICON_SIZE, TOOLTIP_ICON_SIZE);
		icon_params.rect = icon_rect;
		icon_params.follows.flags = FOLLOWS_LEFT | FOLLOWS_BOTTOM;
		icon_params.image = p.image;
		icon_params.mouse_opaque = false;
		addChild(LLUICtrlFactory::create<LLIconCtrl>(icon_params));

		// move text over to fit image in
		mTextBox->translate(TOOLTIP_ICON_SIZE + mPadding, 0);
	}

	if (p.click_callback.isProvided())
	{
		setMouseUpCallback(boost::bind(p.click_callback()));
	}
}

void LLToolTip::setValue(const LLSD& value)
{
	const S32 REALLY_LARGE_HEIGHT = 10000;
	reshape(mMaxWidth, REALLY_LARGE_HEIGHT);

	mTextBox->setValue(value);

	LLRect text_contents_rect = mTextBox->getContentsRect();
	S32 text_width = llmin(mMaxWidth, text_contents_rect.getWidth());
	S32 text_height = text_contents_rect.getHeight();
	mTextBox->reshape(text_width, text_height);

	// reshape tooltip panel to fit text box
	LLRect tooltip_rect = calcBoundingRect();
	tooltip_rect.mTop += mPadding;
	tooltip_rect.mRight += mPadding;
	tooltip_rect.mBottom = 0;
	tooltip_rect.mLeft = 0;

	setRect(tooltip_rect);
}

void LLToolTip::setVisible(BOOL visible)
{
	// fade out tooltip over time
	if (visible)
	{
		mVisibleTimer.start();
		mFadeTimer.stop();
		LLPanel::setVisible(TRUE);
	}
	else
	{
		mVisibleTimer.stop();
		// don't actually change mVisible state, start fade out transition instead
		if (!mFadeTimer.getStarted())
		{
			mFadeTimer.start();
		}
	}
}

BOOL LLToolTip::handleHover(S32 x, S32 y, MASK mask)
{
	LLPanel::handleHover(x, y, mask);
	if (mHasClickCallback)
	{
		getWindow()->setCursor(UI_CURSOR_HAND);
	}
	return TRUE;
}

void LLToolTip::draw()
{
	F32 alpha = 1.f;

	if (mFadeTimer.getStarted())
	{
		F32 tool_tip_fade_time = LLUI::sSettingGroups["config"]->getF32("ToolTipFadeTime");
		alpha = clamp_rescale(mFadeTimer.getElapsedTimeF32(), 0.f, tool_tip_fade_time, 1.f, 0.f);
		if (alpha == 0.f)
		{
			// finished fading out, so hide ourselves
			mFadeTimer.stop();
			LLPanel::setVisible(false);
		}
	}

	// draw tooltip contents with appropriate alpha
	{
		LLViewDrawContext context(alpha);
		LLPanel::draw();
	}
}

bool LLToolTip::isFading() 
{ 
	return mFadeTimer.getStarted(); 
}

F32 LLToolTip::getVisibleTime() 
{ 
	return mVisibleTimer.getStarted() ? mVisibleTimer.getElapsedTimeF32() : 0.f; 
}

bool LLToolTip::hasClickCallback() 
{
	return mHasClickCallback; 
}


//
// LLToolTipMgr
//

LLToolTipMgr::LLToolTipMgr()
:	mToolTip(NULL),
	mNeedsToolTip(false)
{}

void LLToolTipMgr::createToolTip(const LLToolTip::Params& params)
{
	// block all other tooltips until tooltips re-enabled (e.g. mouse moved)
	blockToolTips(); 

	delete mToolTip;

	LLToolTip::Params tooltip_params(params);
	// block mouse events if there is a click handler registered (specifically, hover)
	tooltip_params.mouse_opaque = params.click_callback.isProvided();
	tooltip_params.rect = LLRect (0, 1, 1, 0);

	mToolTip = LLUICtrlFactory::create<LLToolTip> (tooltip_params);
	mToolTip->setValue(params.message());
	gToolTipView->addChild(mToolTip);

	if (params.pos.isProvided())
	{
		LLCoordGL pos = params.pos;
		// try to spawn at requested position
		LLUI::positionViewNearMouse(mToolTip, pos.mX, pos.mY);
	}
	else
	{
		// just spawn at mouse location
		LLUI::positionViewNearMouse(mToolTip);
	}

	//...update "sticky" rect and tooltip position
	if (params.sticky_rect.isProvided())
	{
		mMouseNearRect = params.sticky_rect;
	}
	else
	{
		S32 mouse_x;
		S32 mouse_y;
		LLUI::getMousePositionLocal(gToolTipView->getParent(), &mouse_x, &mouse_y);

		// allow mouse a little bit of slop before changing tooltips
		mMouseNearRect.setCenterAndSize(mouse_x, mouse_y, 3, 3);
	}

	// allow mouse to move all the way to the tooltip without changing tooltips
	// (tooltip can still time out)
	if (mToolTip->hasClickCallback())
	{
		// keep tooltip up when we mouse over it
		mMouseNearRect.unionWith(mToolTip->getRect());
	}
}


void LLToolTipMgr::show(const std::string& msg)
{
	show(LLToolTip::Params().message(msg));
}

void LLToolTipMgr::show(const LLToolTip::Params& params)
{
	if (!params.validateBlock()) 
	{
		llwarns << "Could not display tooltip!" << llendl;
		return;
	}
	
	S32 mouse_x;
	S32 mouse_y;
	LLUI::getMousePositionLocal(gToolTipView, &mouse_x, &mouse_y);

	// are we ready to show the tooltip?
	if (!mToolTipsBlocked									// we haven't hit a key, moved the mouse, etc.
		&& LLUI::getMouseIdleTime() > params.delay_time)	// the mouse has been still long enough
	{
		bool tooltip_changed = mLastToolTipParams.message() != params.message()
								|| mLastToolTipParams.pos() != params.pos();

		bool tooltip_shown = mToolTip 
							 && mToolTip->getVisible() 
							 && !mToolTip->isFading();

		mNeedsToolTip = tooltip_changed || !tooltip_shown;
		// store description of tooltip for later creation
		mNextToolTipParams = params;
	}
}

// allow new tooltips to be created, e.g. after mouse has moved
void LLToolTipMgr::unblockToolTips()
{
	mToolTipsBlocked = false;
}

// disallow new tooltips until unblockTooltips called
void LLToolTipMgr::blockToolTips()
{
	hideToolTips();
	mToolTipsBlocked = true;
}

void LLToolTipMgr::hideToolTips() 
{ 
	if (mToolTip)
	{
		mToolTip->setVisible(FALSE);
	}
}

bool LLToolTipMgr::toolTipVisible()
{
	return mToolTip ? mToolTip->isInVisibleChain() : false;
}

LLRect LLToolTipMgr::getToolTipRect()
{
	if (mToolTip && mToolTip->getVisible())
	{
		return mToolTip->getRect();
	}
	return LLRect();
}


LLRect LLToolTipMgr::getMouseNearRect() 
{ 
	return toolTipVisible() ? mMouseNearRect : LLRect(); 
}

// every frame, determine if current tooltip should be hidden
void LLToolTipMgr::updateToolTipVisibility()
{
	// create new tooltip if we have one ready to go
	if (mNeedsToolTip)
	{
		mNeedsToolTip = false;
		createToolTip(mNextToolTipParams);
		mLastToolTipParams = mNextToolTipParams;
		
		return;
	}

	// hide tooltips when mouse cursor is hidden
	if (LLUI::getWindow()->isCursorHidden())
	{
		blockToolTips();
		return;
	}

	// hide existing tooltips if they have timed out
	S32 mouse_x, mouse_y;
	LLUI::getMousePositionLocal(gToolTipView, &mouse_x, &mouse_y);

	F32 tooltip_timeout = 0.f;
	if (toolTipVisible())
	{
		// mouse far away from tooltip
		tooltip_timeout = mLastToolTipParams.visible_time_far;
		// mouse near rect will only include the tooltip if the 
		// tooltip is clickable
		if (mMouseNearRect.pointInRect(mouse_x, mouse_y))
		{
			// mouse "close" to tooltip
			tooltip_timeout = mLastToolTipParams.visible_time_near;

			// if tooltip is clickable (has large mMouseNearRect)
			// than having cursor over tooltip keeps it up indefinitely
			if (mToolTip->parentPointInView(mouse_x, mouse_y))
			{
				// mouse over tooltip itself, don't time out
				tooltip_timeout = mLastToolTipParams.visible_time_over;
			}
		}

		if (mToolTip->getVisibleTime() > tooltip_timeout)
		{
			hideToolTips();
		}
	}
}



// EOF
