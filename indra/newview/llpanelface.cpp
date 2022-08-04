/** 
 * @file llpanelface.cpp
 * @brief Panel in the tools floater for editing face textures, colors, etc.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

// file include
#include "llpanelface.h"
 
// library includes
#include "llcalc.h"
#include "llerror.h"
#include "llfocusmgr.h"
#include "llrect.h"
#include "llstring.h"
#include "llfontgl.h"

// project includes
#include "llagent.h"
#include "llagentdata.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h" // gInventory
#include "llinventorymodelbackgroundfetch.h"
#include "lllineeditor.h"
#include "llmaterialmgr.h"
#include "llmediaentry.h"
#include "llmenubutton.h"
#include "llnotificationsutil.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "lltooldraganddrop.h"
#include "lltoolface.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvovolume.h"
#include "lluictrlfactory.h"
#include "llpluginclassmedia.h"
#include "llviewertexturelist.h"// Update sel manager as to which channel we're editing so it can reflect the correct overlay UI

//
// Constant definitions for comboboxes
// Must match the commbobox definitions in panel_tools_texture.xml
//
const S32 MATMEDIA_MATERIAL = 0;	// Material
const S32 MATMEDIA_MEDIA = 1;		// Media
const S32 MATTYPE_DIFFUSE = 0;		// Diffuse material texture
const S32 MATTYPE_NORMAL = 1;		// Normal map
const S32 MATTYPE_SPECULAR = 2;		// Specular map
const S32 ALPHAMODE_MASK = 2;		// Alpha masking mode
const S32 BUMPY_TEXTURE = 18;		// use supplied normal map
const S32 SHINY_TEXTURE = 4;		// use supplied specular map

BOOST_STATIC_ASSERT(MATTYPE_DIFFUSE == LLRender::DIFFUSE_MAP && MATTYPE_NORMAL == LLRender::NORMAL_MAP && MATTYPE_SPECULAR == LLRender::SPECULAR_MAP);

//
// "Use texture" label for normal/specular type comboboxes
// Filled in at initialization from translated strings
//
std::string USE_TEXTURE;

LLRender::eTexIndex LLPanelFace::getTextureChannelToEdit()
{
	LLComboBox* combobox_matmedia = getChild<LLComboBox>("combobox matmedia");
	LLRadioGroup* radio_mat_type = getChild<LLRadioGroup>("radio_material_type");

	LLRender::eTexIndex channel_to_edit = (combobox_matmedia && combobox_matmedia->getCurrentIndex() == MATMEDIA_MATERIAL) ?
	                                                    (radio_mat_type ? (LLRender::eTexIndex)radio_mat_type->getSelectedIndex() : LLRender::DIFFUSE_MAP) : LLRender::DIFFUSE_MAP;

	channel_to_edit = (channel_to_edit == LLRender::NORMAL_MAP)		? (getCurrentNormalMap().isNull()		? LLRender::DIFFUSE_MAP : channel_to_edit) : channel_to_edit;
	channel_to_edit = (channel_to_edit == LLRender::SPECULAR_MAP)	? (getCurrentSpecularMap().isNull()		? LLRender::DIFFUSE_MAP : channel_to_edit) : channel_to_edit;
	return channel_to_edit;
}

// Things the UI provides...
//
LLUUID	LLPanelFace::getCurrentNormalMap()			{ return mBumpyTextureCtrl->getImageAssetID();	}
LLUUID	LLPanelFace::getCurrentSpecularMap()		{ return mShinyTextureCtrl->getImageAssetID();	}
U32		LLPanelFace::getCurrentShininess()			{ return getChild<LLComboBox>("combobox shininess")->getCurrentIndex();			}
U32		LLPanelFace::getCurrentBumpiness()			{ return getChild<LLComboBox>("combobox bumpiness")->getCurrentIndex();			}
U8			LLPanelFace::getCurrentDiffuseAlphaMode()	{ return (U8)getChild<LLComboBox>("combobox alphamode")->getCurrentIndex();	}
U8			LLPanelFace::getCurrentAlphaMaskCutoff()	{ return (U8)getChild<LLUICtrl>("maskcutoff")->getValue().asInteger();			}
U8			LLPanelFace::getCurrentEnvIntensity()		{ return (U8)getChild<LLUICtrl>("environment")->getValue().asInteger();			}
U8			LLPanelFace::getCurrentGlossiness()			{ return (U8)getChild<LLUICtrl>("glossiness")->getValue().asInteger();			}
F32		LLPanelFace::getCurrentBumpyRot()			{ return mCtrlBumpyRot->getValue().asReal();						}
F32		LLPanelFace::getCurrentBumpyScaleU()		{ return mCtrlBumpyScaleU->getValue().asReal();					}
F32		LLPanelFace::getCurrentBumpyScaleV()		{ return mCtrlBumpyScaleV->getValue().asReal();					}
F32		LLPanelFace::getCurrentBumpyOffsetU()		{ return mCtrlBumpyOffsetU->getValue().asReal();					}
F32		LLPanelFace::getCurrentBumpyOffsetV()		{ return mCtrlBumpyOffsetV->getValue().asReal();					}
F32		LLPanelFace::getCurrentShinyRot()			{ return mCtrlShinyRot->getValue().asReal();						}
F32		LLPanelFace::getCurrentShinyScaleU()		{ return mCtrlShinyScaleU->getValue().asReal();					}
F32		LLPanelFace::getCurrentShinyScaleV()		{ return mCtrlShinyScaleV->getValue().asReal();					}
F32		LLPanelFace::getCurrentShinyOffsetU()		{ return mCtrlShinyOffsetU->getValue().asReal();					}
F32		LLPanelFace::getCurrentShinyOffsetV()		{ return mCtrlShinyOffsetV->getValue().asReal();					}

// <FS:CR> UI provided diffuse parameters
F32		LLPanelFace::getCurrentTextureRot()			{ return mCtrlTexRot->getValue().asReal();						}
F32		LLPanelFace::getCurrentTextureScaleU()		{ return mCtrlTexScaleU->getValue().asReal();					}
F32		LLPanelFace::getCurrentTextureScaleV()		{ return mCtrlTexScaleV->getValue().asReal();					}
F32		LLPanelFace::getCurrentTextureOffsetU()		{ return mCtrlTexOffsetU->getValue().asReal();					}
F32		LLPanelFace::getCurrentTextureOffsetV()		{ return mCtrlTexOffsetV->getValue().asReal();					}
// </FS:CR>

//
// Methods
//

BOOL	LLPanelFace::postBuild()
{
	childSetCommitCallback("combobox shininess",&LLPanelFace::onCommitShiny,this);
	childSetCommitCallback("combobox bumpiness",&LLPanelFace::onCommitBump,this);
	childSetCommitCallback("combobox alphamode",&LLPanelFace::onCommitAlphaMode,this);
	//childSetCommitCallback("TexScaleU",&LLPanelFace::onCommitTextureScaleX, this);
	//childSetCommitCallback("TexScaleV",&LLPanelFace::onCommitTextureScaleY, this);
	//childSetCommitCallback("TexRot",&LLPanelFace::onCommitTextureRot, this);
	//childSetCommitCallback("rptctrl",&LLPanelFace::onCommitRepeatsPerMeter, this);
	childSetCommitCallback("checkbox planar align",&LLPanelFace::onCommitPlanarAlign, this);
	//childSetCommitCallback("TexOffsetU",LLPanelFace::onCommitTextureOffsetX, this);
	//childSetCommitCallback("TexOffsetV",LLPanelFace::onCommitTextureOffsetY, this);

	//childSetCommitCallback("bumpyScaleU",&LLPanelFace::onCommitMaterialBumpyScaleX, this);
	//childSetCommitCallback("bumpyScaleV",&LLPanelFace::onCommitMaterialBumpyScaleY, this);
	//childSetCommitCallback("bumpyRot",&LLPanelFace::onCommitMaterialBumpyRot, this);
	//childSetCommitCallback("bumpyOffsetU",&LLPanelFace::onCommitMaterialBumpyOffsetX, this);
	//childSetCommitCallback("bumpyOffsetV",&LLPanelFace::onCommitMaterialBumpyOffsetY, this);
	//childSetCommitCallback("shinyScaleU",&LLPanelFace::onCommitMaterialShinyScaleX, this);
	//childSetCommitCallback("shinyScaleV",&LLPanelFace::onCommitMaterialShinyScaleY, this);
	//childSetCommitCallback("shinyRot",&LLPanelFace::onCommitMaterialShinyRot, this);
	//childSetCommitCallback("shinyOffsetU",&LLPanelFace::onCommitMaterialShinyOffsetX, this);
	//childSetCommitCallback("shinyOffsetV",&LLPanelFace::onCommitMaterialShinyOffsetY, this);
	childSetCommitCallback("glossiness",&LLPanelFace::onCommitMaterialGloss, this);
	childSetCommitCallback("environment",&LLPanelFace::onCommitMaterialEnv, this);
	childSetCommitCallback("maskcutoff",&LLPanelFace::onCommitMaterialMaskCutoff, this);

	// <FS:CR>
	childSetCommitCallback("checkbox_sync_settings", &LLPanelFace::onClickMapsSync, this);
	
	mCtrlTexScaleU = getChild<LLSpinCtrl>("TexScaleU");
	if (mCtrlTexScaleU)
	{
		mCtrlTexScaleU->setCommitCallback(&LLPanelFace::onCommitTextureScaleX, this);
	}
	mCtrlTexScaleV = getChild<LLSpinCtrl>("TexScaleV");
	if (mCtrlTexScaleV)
	{
		mCtrlTexScaleV->setCommitCallback(&LLPanelFace::onCommitTextureScaleY, this);
	}
	mCtrlBumpyScaleU = getChild<LLSpinCtrl>("bumpyScaleU");
	if (mCtrlBumpyScaleU)
	{
		mCtrlBumpyScaleU->setCommitCallback(&LLPanelFace::onCommitMaterialBumpyScaleX, this);
	}
	mCtrlBumpyScaleV = getChild<LLSpinCtrl>("bumpyScaleV");
	if (mCtrlBumpyScaleV)
	{
		mCtrlBumpyScaleV->setCommitCallback(&LLPanelFace::onCommitMaterialBumpyScaleY, this);
	}
	mCtrlShinyScaleU = getChild<LLSpinCtrl>("shinyScaleU");
	if (mCtrlShinyScaleU)
	{
		mCtrlShinyScaleU->setCommitCallback(&LLPanelFace::onCommitMaterialShinyScaleX, this);
	}
	mCtrlShinyScaleV = getChild<LLSpinCtrl>("shinyScaleV");
	if (mCtrlShinyScaleV)
	{
		mCtrlShinyScaleV->setCommitCallback(&LLPanelFace::onCommitMaterialShinyScaleY, this);
	}	
	mCtrlTexOffsetU = getChild<LLSpinCtrl>("TexOffsetU");
	if (mCtrlTexOffsetU)
	{
		mCtrlTexOffsetU->setCommitCallback(&LLPanelFace::onCommitTextureOffsetX, this);
	}
	mCtrlTexOffsetV = getChild<LLSpinCtrl>("TexOffsetV");
	if (mCtrlTexOffsetV)
	{
		mCtrlTexOffsetV->setCommitCallback(&LLPanelFace::onCommitTextureOffsetY, this);
	}
	mCtrlBumpyOffsetU = getChild<LLSpinCtrl>("bumpyOffsetU");
	if (mCtrlBumpyOffsetU)
	{
		mCtrlBumpyOffsetU->setCommitCallback(&LLPanelFace::onCommitMaterialBumpyOffsetX, this);
	}
	mCtrlBumpyOffsetV = getChild<LLSpinCtrl>("bumpyOffsetV");
	if (mCtrlBumpyOffsetV)
	{
		mCtrlBumpyOffsetV->setCommitCallback(&LLPanelFace::onCommitMaterialBumpyOffsetY, this);
	}
	mCtrlShinyOffsetU = getChild<LLSpinCtrl>("shinyOffsetU");
	if (mCtrlShinyOffsetU)
	{
		mCtrlShinyOffsetU->setCommitCallback(&LLPanelFace::onCommitMaterialShinyOffsetX, this);
	}
	mCtrlShinyOffsetV = getChild<LLSpinCtrl>("shinyOffsetV");
	if (mCtrlShinyOffsetV)
	{
		mCtrlShinyOffsetV->setCommitCallback(&LLPanelFace::onCommitMaterialShinyOffsetY, this);
	}
	mCtrlTexRot = getChild<LLSpinCtrl>("TexRot");
	if (mCtrlTexRot)
	{
		mCtrlTexRot->setCommitCallback(&LLPanelFace::onCommitTextureRot, this);
	}
	mCtrlBumpyRot = getChild<LLSpinCtrl>("bumpyRot");
	if (mCtrlBumpyRot)
	{
		mCtrlBumpyRot->setCommitCallback(&LLPanelFace::onCommitMaterialBumpyRot, this);
	}
	mCtrlShinyRot = getChild<LLSpinCtrl>("shinyRot");
	if (mCtrlShinyRot)
	{
		mCtrlShinyRot->setCommitCallback(&LLPanelFace::onCommitMaterialShinyRot, this);
	}
	mCtrlRpt = getChild<LLSpinCtrl>("rptctrl");
	if (mCtrlRpt)
	{
		mCtrlRpt->setCommitCallback(LLPanelFace::onCommitRepeatsPerMeter, this);
	}
	
	changePrecision(gSavedSettings.getS32("FSBuildToolDecimalPrecision"));
	// </FS>

	childSetAction("button align",&LLPanelFace::onClickAutoFix,this);
	childSetAction("button align textures", &LLPanelFace::onAlignTexture, this);

	// <FS:CR> Moved to the header so other functions can use them too.
	//LLTextureCtrl*	mTextureCtrl;
	//LLTextureCtrl*	mShinyTextureCtrl;
	//LLTextureCtrl*	mBumpyTextureCtrl;
	//LLColorSwatchCtrl*	mColorSwatch;
	//LLColorSwatchCtrl*	mShinyColorSwatch;

	//LLComboBox*		mComboTexGen;
	//LLComboBox*		mComboMatMedia;

	//LLCheckBoxCtrl	*mCheckFullbright;
	
	//LLTextBox*		mLabelColorTransp;
	//LLSpinCtrl*		mCtrlColorTransp;		// transparency = 1 - alpha

	//LLSpinCtrl*     mCtrlGlow;

	setMouseOpaque(FALSE);

	mTextureCtrl = getChild<LLTextureCtrl>("texture control");
	if(mTextureCtrl)
	{
		mTextureCtrl->setDefaultImageAssetID(LLUUID( gSavedSettings.getString( "DefaultObjectTexture" )));
		mTextureCtrl->setCommitCallback( boost::bind(&LLPanelFace::onCommitTexture, this, _2) );
		mTextureCtrl->setOnCancelCallback( boost::bind(&LLPanelFace::onCancelTexture, this, _2) );
		mTextureCtrl->setOnSelectCallback( boost::bind(&LLPanelFace::onSelectTexture, this, _2) );
		mTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mTextureCtrl->setOnCloseCallback( boost::bind(&LLPanelFace::onCloseTexturePicker, this, _2) );

		mTextureCtrl->setFollowsTop();
		mTextureCtrl->setFollowsLeft();
		mTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}

	mShinyTextureCtrl = getChild<LLTextureCtrl>("shinytexture control");
	if(mShinyTextureCtrl)
	{
		mShinyTextureCtrl->setDefaultImageAssetID(LLUUID( gSavedSettings.getString( "DefaultObjectSpecularTexture" )));
		mShinyTextureCtrl->setCommitCallback( boost::bind(&LLPanelFace::onCommitSpecularTexture, this, _2) );
		mShinyTextureCtrl->setOnCancelCallback( boost::bind(&LLPanelFace::onCancelSpecularTexture, this, _2) );
		mShinyTextureCtrl->setOnSelectCallback( boost::bind(&LLPanelFace::onSelectSpecularTexture, this, _2) );
		mShinyTextureCtrl->setOnCloseCallback( boost::bind(&LLPanelFace::onCloseTexturePicker, this, _2) );
		
		mShinyTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mShinyTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mShinyTextureCtrl->setFollowsTop();
		mShinyTextureCtrl->setFollowsLeft();
		mShinyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mShinyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}

	mBumpyTextureCtrl = getChild<LLTextureCtrl>("bumpytexture control");
	if(mBumpyTextureCtrl)
	{
		mBumpyTextureCtrl->setDefaultImageAssetID(LLUUID( gSavedSettings.getString( "DefaultObjectNormalTexture" )));
		mBumpyTextureCtrl->setBlankImageAssetID(LLUUID( gSavedSettings.getString( "DefaultBlankNormalTexture" )));
		mBumpyTextureCtrl->setCommitCallback( boost::bind(&LLPanelFace::onCommitNormalTexture, this, _2) );
		mBumpyTextureCtrl->setOnCancelCallback( boost::bind(&LLPanelFace::onCancelNormalTexture, this, _2) );
		mBumpyTextureCtrl->setOnSelectCallback( boost::bind(&LLPanelFace::onSelectNormalTexture, this, _2) );
		mBumpyTextureCtrl->setOnCloseCallback( boost::bind(&LLPanelFace::onCloseTexturePicker, this, _2) );

		mBumpyTextureCtrl->setDragCallback(boost::bind(&LLPanelFace::onDragTexture, this, _2));
		mBumpyTextureCtrl->setOnTextureSelectedCallback(boost::bind(&LLPanelFace::onTextureSelectionChanged, this, _1));
		mBumpyTextureCtrl->setFollowsTop();
		mBumpyTextureCtrl->setFollowsLeft();
		mBumpyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
		mBumpyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
	}

	mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
	if(mColorSwatch)
	{
		mColorSwatch->setCommitCallback(boost::bind(&LLPanelFace::onCommitColor, this, _2));
		mColorSwatch->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelColor, this, _2));
		mColorSwatch->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectColor, this, _2));
		mColorSwatch->setFollowsTop();
		mColorSwatch->setFollowsLeft();
		mColorSwatch->setCanApplyImmediately(TRUE);
	}

	mShinyColorSwatch = getChild<LLColorSwatchCtrl>("shinycolorswatch");
	if(mShinyColorSwatch)
	{
		mShinyColorSwatch->setCommitCallback(boost::bind(&LLPanelFace::onCommitShinyColor, this, _2));
		mShinyColorSwatch->setOnCancelCallback(boost::bind(&LLPanelFace::onCancelShinyColor, this, _2));
		mShinyColorSwatch->setOnSelectCallback(boost::bind(&LLPanelFace::onSelectShinyColor, this, _2));
		mShinyColorSwatch->setFollowsTop();
		mShinyColorSwatch->setFollowsLeft();
		mShinyColorSwatch->setCanApplyImmediately(TRUE);
	}

	mLabelColorTransp = getChild<LLTextBox>("color trans");
	if(mLabelColorTransp)
	{
		mLabelColorTransp->setFollowsTop();
		mLabelColorTransp->setFollowsLeft();
	}

	mCtrlColorTransp = getChild<LLSpinCtrl>("ColorTrans");
	if(mCtrlColorTransp)
	{
		mCtrlColorTransp->setCommitCallback(boost::bind(&LLPanelFace::onCommitAlpha, this, _2));
		mCtrlColorTransp->setPrecision(0);
		mCtrlColorTransp->setFollowsTop();
		mCtrlColorTransp->setFollowsLeft();
	}

	mCheckFullbright = getChild<LLCheckBoxCtrl>("checkbox fullbright");
	if (mCheckFullbright)
	{
		mCheckFullbright->setCommitCallback(LLPanelFace::onCommitFullbright, this);
	}

	mComboTexGen = getChild<LLComboBox>("combobox texgen");
	if(mComboTexGen)
	{
		mComboTexGen->setCommitCallback(LLPanelFace::onCommitTexGen, this);
		mComboTexGen->setFollows(FOLLOWS_LEFT | FOLLOWS_TOP);	
	}

	mComboMatMedia = getChild<LLComboBox>("combobox matmedia");
	if(mComboMatMedia)
	{
		mComboMatMedia->setCommitCallback(LLPanelFace::onCommitMaterialsMedia,this);
		mComboMatMedia->selectNthItem(MATMEDIA_MATERIAL);
	}

	mRadioMatType = getChild<LLRadioGroup>("radio_material_type");
    if(mRadioMatType)
    {
        mRadioMatType->setCommitCallback(LLPanelFace::onCommitMaterialType, this);
        mRadioMatType->selectNthItem(MATTYPE_DIFFUSE);
    }

	mCtrlGlow = getChild<LLSpinCtrl>("glow");
	if(mCtrlGlow)
	{
		mCtrlGlow->setCommitCallback(LLPanelFace::onCommitGlow, this);
	}

    // <FS> Extended copy & paste buttons
    //mMenuClipboardColor = getChild<LLMenuButton>("clipboard_color_params_btn");
    //mMenuClipboardTexture = getChild<LLMenuButton>("clipboard_texture_params_btn");
    mBtnCopyFaces = getChild<LLButton>("copy_face_btn");
    mBtnCopyFaces->setCommitCallback(boost::bind(&LLPanelFace::onCopyFaces, this));
    mBtnPasteFaces = getChild<LLButton>("paste_face_btn");
    mBtnPasteFaces->setCommitCallback(boost::bind(&LLPanelFace::onPasteFaces, this));
    // </FS>

	clearCtrls();

	return TRUE;
}

LLPanelFace::LLPanelFace()
:	LLPanel(),
	mIsAlpha(false)
{
    USE_TEXTURE = LLTrans::getString("use_texture");
    // <FS> Extended copy & paste buttons
    //mCommitCallbackRegistrar.add("PanelFace.menuDoToSelected", boost::bind(&LLPanelFace::menuDoToSelected, this, _2));
    //mEnableCallbackRegistrar.add("PanelFace.menuEnable", boost::bind(&LLPanelFace::menuEnableItem, this, _2));
    // </FS>
}


LLPanelFace::~LLPanelFace()
{
	// Children all cleaned up by default view destructor.
}


void LLPanelFace::draw()
{
    updateCopyTexButton();

    LLPanel::draw();
}

void LLPanelFace::sendTexture()
{
	//LLTextureCtrl* mTextureCtrl = getChild<LLTextureCtrl>("texture control");
	if(!mTextureCtrl) return;
	if( !mTextureCtrl->getTentative() )
	{
		// we grab the item id first, because we want to do a
		// permissions check in the selection manager. ARGH!
		LLUUID id = mTextureCtrl->getImageItemID();
		if(id.isNull())
		{
			id = mTextureCtrl->getImageAssetID();
		}
		LLSelectMgr::getInstance()->selectionSetImage(id);
	}
}

void LLPanelFace::sendBump(U32 bumpiness)
{	
	//LLTextureCtrl* bumpytexture_ctrl = getChild<LLTextureCtrl>("bumpytexture control");
	if (!mBumpyTextureCtrl) return;
	if (bumpiness < BUMPY_TEXTURE)
{	
		LL_DEBUGS("Materials") << "clearing bumptexture control" << LL_ENDL;	
		//bumpytexture_ctrl->clear();
		//bumpytexture_ctrl->setImageAssetID(LLUUID());
		mBumpyTextureCtrl->clear();
		mBumpyTextureCtrl->setImageAssetID(LLUUID());
	}

	updateBumpyControls(bumpiness == BUMPY_TEXTURE, true);

	LLUUID current_normal_map = mBumpyTextureCtrl->getImageAssetID();

	U8 bump = (U8) bumpiness & TEM_BUMP_MASK;

	// Clear legacy bump to None when using an actual normal map
	//
	if (!current_normal_map.isNull())
		bump = 0;

	// Set the normal map or reset it to null as appropriate
	//
	LLSelectedTEMaterial::setNormalID(this, current_normal_map);

	//LLSelectMgr::getInstance()->selectionSetBumpmap(bump, bumpytexture_ctrl->getImageItemID());
	LLSelectMgr::getInstance()->selectionSetBumpmap(bump, mBumpyTextureCtrl->getImageItemID());
}

void LLPanelFace::sendTexGen()
{
	//LLComboBox*	mComboTexGen = getChild<LLComboBox>("combobox texgen");
	if(!mComboTexGen)return;
	U8 tex_gen = (U8) mComboTexGen->getCurrentIndex() << TEM_TEX_GEN_SHIFT;
	LLSelectMgr::getInstance()->selectionSetTexGen( tex_gen );
}

void LLPanelFace::sendShiny(U32 shininess)
{
	//LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>("shinytexture control");
	if (!mShinyTextureCtrl) return;
	
	if (shininess < SHINY_TEXTURE)
{
		//texture_ctrl->clear();
		//texture_ctrl->setImageAssetID(LLUUID());
		mShinyTextureCtrl->clear();
		mShinyTextureCtrl->setImageAssetID(LLUUID());
	}

	LLUUID specmap = getCurrentSpecularMap();

	U8 shiny = (U8) shininess & TEM_SHINY_MASK;
	if (!specmap.isNull())
		shiny = 0;

	LLSelectedTEMaterial::setSpecularID(this, specmap);

	//LLSelectMgr::getInstance()->selectionSetShiny(shiny, texture_ctrl->getImageItemID());
	LLSelectMgr::getInstance()->selectionSetShiny(shiny, mShinyTextureCtrl->getImageItemID());

	updateShinyControls(!specmap.isNull(), true);
	
}

void LLPanelFace::sendFullbright()
{
	//LLCheckBoxCtrl*	mCheckFullbright = getChild<LLCheckBoxCtrl>("checkbox fullbright");
	if (!mCheckFullbright) return;
	U8 fullbright = mCheckFullbright->get() ? TEM_FULLBRIGHT_MASK : 0;
	LLSelectMgr::getInstance()->selectionSetFullbright( fullbright );
}

void LLPanelFace::sendColor()
{
	//LLColorSwatchCtrl*	mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
	if (!mColorSwatch) return;
	LLColor4 color = mColorSwatch->get();

	LLSelectMgr::getInstance()->selectionSetColorOnly( color );
}

void LLPanelFace::sendAlpha()
{	
	//LLSpinCtrl*	mCtrlColorTransp = getChild<LLSpinCtrl>("ColorTrans");
	if (!mCtrlColorTransp) return;
	F32 alpha = (100.f - mCtrlColorTransp->get()) / 100.f;

	LLSelectMgr::getInstance()->selectionSetAlphaOnly( alpha );
}


void LLPanelFace::sendGlow()
{
	//LLSpinCtrl* mCtrlGlow = getChild<LLSpinCtrl>("glow");
	//llassert(mCtrlGlow);
	if (!mCtrlGlow) return;
	if (mCtrlGlow)
	{
		F32 glow = mCtrlGlow->get();
		LLSelectMgr::getInstance()->selectionSetGlow( glow );
	}
}

struct LLPanelFaceSetTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceSetTEFunctor(LLPanelFace* panel) : mPanel(panel) {}
	virtual bool apply(LLViewerObject* object, S32 te)
	{
		BOOL valid;
		F32 value;

        LLRadioGroup * radio_mat_type = mPanel->getChild<LLRadioGroup>("radio_material_type");
        std::string prefix;
        switch (radio_mat_type->getSelectedIndex())
        {
        case MATTYPE_DIFFUSE:
            prefix = "Tex";
            break;
        case MATTYPE_NORMAL:
            prefix = "bumpy";
            break;
        case MATTYPE_SPECULAR:
            prefix = "shiny";
            break;
        }
        
        LLSpinCtrl * ctrlTexScaleS = mPanel->getChild<LLSpinCtrl>(prefix + "ScaleU");
        LLSpinCtrl * ctrlTexScaleT = mPanel->getChild<LLSpinCtrl>(prefix + "ScaleV");
        LLSpinCtrl * ctrlTexOffsetS = mPanel->getChild<LLSpinCtrl>(prefix + "OffsetU");
        LLSpinCtrl * ctrlTexOffsetT = mPanel->getChild<LLSpinCtrl>(prefix + "OffsetV");
        LLSpinCtrl * ctrlTexRotation = mPanel->getChild<LLSpinCtrl>(prefix + "Rot");

		LLComboBox*	comboTexGen = mPanel->getChild<LLComboBox>("combobox texgen");
		LLCheckBoxCtrl*	cb_planar_align = mPanel->getChild<LLCheckBoxCtrl>("checkbox planar align");
		bool align_planar = (cb_planar_align && cb_planar_align->get());

		llassert(comboTexGen);
		llassert(object);

		if (ctrlTexScaleS)
		{
			valid = !ctrlTexScaleS->getTentative();
			if (valid || align_planar)
			{
				value = ctrlTexScaleS->get();
				if (comboTexGen &&
				    comboTexGen->getCurrentIndex() == 1)
				{
					value *= 0.5f;
				}
				object->setTEScaleS( te, value );

				if (align_planar) 
				{
					LLPanelFace::LLSelectedTEMaterial::setNormalRepeatX(mPanel, value, te, object->getID());
					LLPanelFace::LLSelectedTEMaterial::setSpecularRepeatX(mPanel, value, te, object->getID());
				}
			}
		}

		if (ctrlTexScaleT)
		{
			valid = !ctrlTexScaleT->getTentative();
			if (valid || align_planar)
			{
				value = ctrlTexScaleT->get();
				if (comboTexGen &&
				    comboTexGen->getCurrentIndex() == 1)
				{
					value *= 0.5f;
				}
				object->setTEScaleT( te, value );

				if (align_planar) 
				{
					LLPanelFace::LLSelectedTEMaterial::setNormalRepeatY(mPanel, value, te, object->getID());
					LLPanelFace::LLSelectedTEMaterial::setSpecularRepeatY(mPanel, value, te, object->getID());
				}
			}
		}

		if (ctrlTexOffsetS)
		{
			valid = !ctrlTexOffsetS->getTentative();
			if (valid || align_planar)
			{
				value = ctrlTexOffsetS->get();
				object->setTEOffsetS( te, value );

				if (align_planar) 
				{
					LLPanelFace::LLSelectedTEMaterial::setNormalOffsetX(mPanel, value, te, object->getID());
					LLPanelFace::LLSelectedTEMaterial::setSpecularOffsetX(mPanel, value, te, object->getID());
				}
			}
		}

		if (ctrlTexOffsetT)
		{
			valid = !ctrlTexOffsetT->getTentative();
			if (valid || align_planar)
			{
				value = ctrlTexOffsetT->get();
				object->setTEOffsetT( te, value );

				if (align_planar) 
				{
					LLPanelFace::LLSelectedTEMaterial::setNormalOffsetY(mPanel, value, te, object->getID());
					LLPanelFace::LLSelectedTEMaterial::setSpecularOffsetY(mPanel, value, te, object->getID());
				}
			}
		}

		if (ctrlTexRotation)
		{
			valid = !ctrlTexRotation->getTentative();
			if (valid || align_planar)
			{
				value = ctrlTexRotation->get() * DEG_TO_RAD;
				object->setTERotation( te, value );

				if (align_planar) 
				{
					LLPanelFace::LLSelectedTEMaterial::setNormalRotation(mPanel, value, te, object->getID());
					LLPanelFace::LLSelectedTEMaterial::setSpecularRotation(mPanel, value, te, object->getID());
				}
			}
		}
		return true;
	}
private:
	LLPanelFace* mPanel;
};

// Functor that aligns a face to mCenterFace
struct LLPanelFaceSetAlignedTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceSetAlignedTEFunctor(LLPanelFace* panel, LLFace* center_face) :
		mPanel(panel),
		mCenterFace(center_face) {}

	virtual bool apply(LLViewerObject* object, S32 te)
	{
		LLFace* facep = object->mDrawable->getFace(te);
		if (!facep)
		{
			return true;
		}

		if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{
			return true;
		}

		bool set_aligned = true;
		if (facep == mCenterFace)
		{
			set_aligned = false;
		}
		if (set_aligned)
		{
			LLVector2 uv_offset, uv_scale;
			F32 uv_rot;
			set_aligned = facep->calcAlignedPlanarTE(mCenterFace, &uv_offset, &uv_scale, &uv_rot);
			if (set_aligned)
			{
				object->setTEOffset(te, uv_offset.mV[VX], uv_offset.mV[VY]);
				object->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
				object->setTERotation(te, uv_rot);

				LLPanelFace::LLSelectedTEMaterial::setNormalRotation(mPanel, uv_rot, te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setSpecularRotation(mPanel, uv_rot, te, object->getID());

				LLPanelFace::LLSelectedTEMaterial::setNormalOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setNormalOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setNormalRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setNormalRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());

				LLPanelFace::LLSelectedTEMaterial::setSpecularOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setSpecularOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setSpecularRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
				LLPanelFace::LLSelectedTEMaterial::setSpecularRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());
			}
		}
		if (!set_aligned)
		{
			LLPanelFaceSetTEFunctor setfunc(mPanel);
			setfunc.apply(object, te);
		}
		return true;
	}
private:
	LLPanelFace* mPanel;
	LLFace* mCenterFace;
};

struct LLPanelFaceSetAlignedConcreteTEFunctor : public LLSelectedTEFunctor
{
    LLPanelFaceSetAlignedConcreteTEFunctor(LLPanelFace* panel, LLFace* center_face, LLRender::eTexIndex map) :
        mPanel(panel),
        mChefFace(center_face),
        mMap(map)
    {}

    virtual bool apply(LLViewerObject* object, S32 te)
    {
        LLFace* facep = object->mDrawable->getFace(te);
        if (!facep)
        {
            return true;
        }

        if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
        {
            return true;
        }

        if (mChefFace != facep)
        {
            LLVector2 uv_offset, uv_scale;
            F32 uv_rot;
            if (facep->calcAlignedPlanarTE(mChefFace, &uv_offset, &uv_scale, &uv_rot, mMap))
            {
                switch (mMap)
                {
                case LLRender::DIFFUSE_MAP:
                        object->setTEOffset(te, uv_offset.mV[VX], uv_offset.mV[VY]);
                        object->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
                        object->setTERotation(te, uv_rot);
                    break;
                case LLRender::NORMAL_MAP:
                        LLPanelFace::LLSelectedTEMaterial::setNormalRotation(mPanel, uv_rot, te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setNormalOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setNormalOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setNormalRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setNormalRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());
                    break;
                case LLRender::SPECULAR_MAP:
                        LLPanelFace::LLSelectedTEMaterial::setSpecularRotation(mPanel, uv_rot, te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setSpecularOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setSpecularOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setSpecularRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
                        LLPanelFace::LLSelectedTEMaterial::setSpecularRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());
                    break;
                default: /*make compiler happy*/
                    break;
                }
            }
        }
        
        return true;
    }
private:
    LLPanelFace* mPanel;
    LLFace* mChefFace;
    LLRender::eTexIndex mMap;
};

// Functor that tests if a face is aligned to mCenterFace
struct LLPanelFaceGetIsAlignedTEFunctor : public LLSelectedTEFunctor
{
	LLPanelFaceGetIsAlignedTEFunctor(LLFace* center_face) :
		mCenterFace(center_face) {}

	virtual bool apply(LLViewerObject* object, S32 te)
	{
		LLFace* facep = object->mDrawable->getFace(te);
		if (!facep)
		{
			return false;
		}

		if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
		{ //volume face does not exist, can't be aligned
			return false;
		}

		if (facep == mCenterFace)
		{
			return true;
		}
		
		LLVector2 aligned_st_offset, aligned_st_scale;
		F32 aligned_st_rot;
		if ( facep->calcAlignedPlanarTE(mCenterFace, &aligned_st_offset, &aligned_st_scale, &aligned_st_rot) )
		{
			const LLTextureEntry* tep = facep->getTextureEntry();
			LLVector2 st_offset, st_scale;
			tep->getOffset(&st_offset.mV[VX], &st_offset.mV[VY]);
			tep->getScale(&st_scale.mV[VX], &st_scale.mV[VY]);
			F32 st_rot = tep->getRotation();

            bool eq_offset_x = is_approx_equal_fraction(st_offset.mV[VX], aligned_st_offset.mV[VX], 12);
            bool eq_offset_y = is_approx_equal_fraction(st_offset.mV[VY], aligned_st_offset.mV[VY], 12);
            bool eq_scale_x  = is_approx_equal_fraction(st_scale.mV[VX], aligned_st_scale.mV[VX], 12);
            bool eq_scale_y  = is_approx_equal_fraction(st_scale.mV[VY], aligned_st_scale.mV[VY], 12);
            bool eq_rot      = is_approx_equal_fraction(st_rot, aligned_st_rot, 6);

			// needs a fuzzy comparison, because of fp errors
			if (eq_offset_x && 
				eq_offset_y && 
				eq_scale_x &&
				eq_scale_y &&
				eq_rot)
			{
				return true;
			}
		}
		return false;
	}
private:
	LLFace* mCenterFace;
};

struct LLPanelFaceSendFunctor : public LLSelectedObjectFunctor
{
	virtual bool apply(LLViewerObject* object)
	{
		object->sendTEUpdate();
		return true;
	}
};

void LLPanelFace::sendTextureInfo()
{
	if ((bool)childGetValue("checkbox planar align").asBoolean())
	{
		LLFace* last_face = NULL;
		bool identical_face =false;
		LLSelectedTE::getFace(last_face, identical_face);		
		LLPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
		LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
	}
	else
	{
		LLPanelFaceSetTEFunctor setfunc(this);
		LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
	}

	LLPanelFaceSendFunctor sendfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}

void LLPanelFace::alignTestureLayer()
{
    LLFace* last_face = NULL;
    bool identical_face = false;
    LLSelectedTE::getFace(last_face, identical_face);

    LLRadioGroup * radio_mat_type = getChild<LLRadioGroup>("radio_material_type");
    LLPanelFaceSetAlignedConcreteTEFunctor setfunc(this, last_face, static_cast<LLRender::eTexIndex>(radio_mat_type->getSelectedIndex()));
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
}

void LLPanelFace::getState()
{
	updateUI();
}

void LLPanelFace::updateUI(bool force_set_values /*false*/)
{ //set state of UI to match state of texture entry(ies)  (calls setEnabled, setValue, etc, but NOT setVisible)
	LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

	if( objectp
		&& objectp->getPCode() == LL_PCODE_VOLUME
		&& objectp->permModify())
	{
		BOOL editable = objectp->permModify() && !objectp->isPermanentEnforced();

		// only turn on auto-adjust button if there is a media renderer and the media is loaded
		getChildView("button align")->setEnabled(editable);
		
		// <FS>
		BOOL enable_material_controls = (!gSavedSettings.getBOOL("SyncMaterialSettings"));

		//LLComboBox* combobox_matmedia = getChild<LLComboBox>("combobox matmedia");
		if (mComboMatMedia)
		{
			if (mComboMatMedia->getCurrentIndex() < MATMEDIA_MATERIAL)
			{
				mComboMatMedia->selectNthItem(MATMEDIA_MATERIAL);
			}
		}
		else
		{
			LL_WARNS() << "failed getChild for 'combobox matmedia'" << LL_ENDL;
		}
		mComboMatMedia->setEnabled(editable);

		//LLRadioGroup* radio_mat_type = getChild<LLRadioGroup>("radio_material_type");
		if(mRadioMatType)
		{
		    if (mRadioMatType->getSelectedIndex() < MATTYPE_DIFFUSE)
		    {
		        mRadioMatType->selectNthItem(MATTYPE_DIFFUSE);
		    }

		}
		else
		{
		    LL_WARNS("Materials") << "failed getChild for 'radio_material_type'" << LL_ENDL;
		}

		mRadioMatType->setEnabled(editable);
		getChildView("checkbox_sync_settings")->setEnabled(editable);
		childSetValue("checkbox_sync_settings", gSavedSettings.getBOOL("SyncMaterialSettings"));
		updateVisibility();

		bool identical			= true;	// true because it is anded below
		bool identical_diffuse	= false;
		bool identical_norm		= false;
		bool identical_spec		= false;

		//LLTextureCtrl*	texture_ctrl = getChild<LLTextureCtrl>("texture control");
		//LLTextureCtrl*	shinytexture_ctrl = getChild<LLTextureCtrl>("shinytexture control");
		//LLTextureCtrl*	bumpytexture_ctrl = getChild<LLTextureCtrl>("bumpytexture control");
		
		LLUUID id;
		LLUUID normmap_id;
		LLUUID specmap_id;
		
		// Color swatch
		{
			getChildView("color label")->setEnabled(editable);
		}
		//LLColorSwatchCtrl*	mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");

		LLColor4 color					= LLColor4::white;
		bool		identical_color	= false;

		if(mColorSwatch)
		{
			LLSelectedTE::getColor(color, identical_color);
			LLColor4 prev_color = mColorSwatch->get();

			mColorSwatch->setOriginal(color);
			mColorSwatch->set(color, force_set_values || (prev_color != color) || !editable);

			mColorSwatch->setValid(editable);
			mColorSwatch->setEnabled( editable );
			mColorSwatch->setCanApplyImmediately( editable );
		}

		// Color transparency
		getChildView("color trans")->setEnabled(editable);

		F32 transparency = (1.f - color.mV[VALPHA]) * 100.f;
		getChild<LLUICtrl>("ColorTrans")->setValue(editable ? transparency : 0);
		getChildView("ColorTrans")->setEnabled(editable);

		// Specular map
		LLSelectedTEMaterial::getSpecularID(specmap_id, identical_spec);
		
		U8 shiny = 0;
		bool identical_shiny = false;

		// Shiny
		LLSelectedTE::getShiny(shiny, identical_shiny);
		identical = identical && identical_shiny;

		shiny = specmap_id.isNull() ? shiny : SHINY_TEXTURE;

		LLCtrlSelectionInterface* combobox_shininess = childGetSelectionInterface("combobox shininess");
		if (combobox_shininess)
				{
			combobox_shininess->selectNthItem((S32)shiny);
		}

		getChildView("label shininess")->setEnabled(editable);
		getChildView("combobox shininess")->setEnabled(editable);

		getChildView("label glossiness")->setEnabled(editable);			
		getChildView("glossiness")->setEnabled(editable);

		getChildView("label environment")->setEnabled(editable);
		getChildView("environment")->setEnabled(editable);
		getChildView("label shinycolor")->setEnabled(editable);
					
		getChild<LLUICtrl>("combobox shininess")->setTentative(!identical_spec);
		getChild<LLUICtrl>("glossiness")->setTentative(!identical_spec);
		getChild<LLUICtrl>("environment")->setTentative(!identical_spec);			
		getChild<LLUICtrl>("shinycolorswatch")->setTentative(!identical_spec);
					
		//LLColorSwatchCtrl*	mShinyColorSwatch = getChild<LLColorSwatchCtrl>("shinycolorswatch");
		if(mShinyColorSwatch)
					{
			mShinyColorSwatch->setValid(editable);
			mShinyColorSwatch->setEnabled( editable );
			mShinyColorSwatch->setCanApplyImmediately( editable );
		}

		U8 bumpy = 0;
		// Bumpy
						{
			bool identical_bumpy = false;
			LLSelectedTE::getBumpmap(bumpy,identical_bumpy);

			LLUUID norm_map_id = getCurrentNormalMap();
			LLCtrlSelectionInterface* combobox_bumpiness = childGetSelectionInterface("combobox bumpiness");

			bumpy = norm_map_id.isNull() ? bumpy : BUMPY_TEXTURE;

			if (combobox_bumpiness)
							{
				combobox_bumpiness->selectNthItem((S32)bumpy);
							}
			else
							{
				LL_WARNS() << "failed childGetSelectionInterface for 'combobox bumpiness'" << LL_ENDL;
							}

			getChildView("combobox bumpiness")->setEnabled(editable);
			getChild<LLUICtrl>("combobox bumpiness")->setTentative(!identical_bumpy);
			getChildView("label bumpiness")->setEnabled(editable);
						}

		// Texture
		{
			LLSelectedTE::getTexId(id,identical_diffuse);

			// Normal map
			LLSelectedTEMaterial::getNormalID(normmap_id, identical_norm);

			mIsAlpha = FALSE;
			LLGLenum image_format = GL_RGB;
			bool identical_image_format = false;
			LLSelectedTE::getImageFormat(image_format, identical_image_format);
            
         mIsAlpha = FALSE;
         switch (image_format)
         {
               case GL_RGBA:
               case GL_ALPHA:
               {
                  mIsAlpha = TRUE;
               }
               break;

               case GL_RGB: break;
               default:
               {
                  LL_WARNS() << "Unexpected tex format in LLPanelFace...resorting to no alpha" << LL_ENDL;
					}
               break;
				}

			if(LLViewerMedia::getInstance()->textureHasMedia(id))
			{
				getChildView("button align")->setEnabled(editable);
			}
			
			// Diffuse Alpha Mode

			// Init to the default that is appropriate for the alpha content of the asset
			//
			U8 alpha_mode = mIsAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

			bool identical_alpha_mode = false;

			// See if that's been overridden by a material setting for same...
			//
			LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(alpha_mode, identical_alpha_mode, mIsAlpha);

			LLCtrlSelectionInterface* combobox_alphamode = childGetSelectionInterface("combobox alphamode");
			if (combobox_alphamode)
			{
				//it is invalid to have any alpha mode other than blend if transparency is greater than zero ... 
				// Want masking? Want emissive? Tough! You get BLEND!
				alpha_mode = (transparency > 0.f) ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : alpha_mode;

				// ... unless there is no alpha channel in the texture, in which case alpha mode MUST be none
				alpha_mode = mIsAlpha ? alpha_mode : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

				combobox_alphamode->selectNthItem(alpha_mode);
			}
			else
			{
				LL_WARNS() << "failed childGetSelectionInterface for 'combobox alphamode'" << LL_ENDL;
			}

			updateAlphaControls();

			if (mTextureCtrl)
			{
				if (identical_diffuse)
				{
					mTextureCtrl->setTentative(FALSE);
					mTextureCtrl->setEnabled(editable);
					mTextureCtrl->setImageAssetID(id);
					getChildView("combobox alphamode")->setEnabled(editable && mIsAlpha && transparency <= 0.f);
					getChildView("label alphamode")->setEnabled(editable && mIsAlpha);
					getChildView("maskcutoff")->setEnabled(editable && mIsAlpha);
					getChildView("label maskcutoff")->setEnabled(editable && mIsAlpha);

					mTextureCtrl->setBakeTextureEnabled(TRUE);
				}
				else if (id.isNull())
				{
					// None selected
					mTextureCtrl->setTentative(FALSE);
					mTextureCtrl->setEnabled(FALSE);
					mTextureCtrl->setImageAssetID(LLUUID::null);
					getChildView("combobox alphamode")->setEnabled(FALSE);
					getChildView("label alphamode")->setEnabled(FALSE);
					getChildView("maskcutoff")->setEnabled(FALSE);
					getChildView("label maskcutoff")->setEnabled(FALSE);

					mTextureCtrl->setBakeTextureEnabled(false);
				}
				else
				{
					// Tentative: multiple selected with different textures
					mTextureCtrl->setTentative(TRUE);
					mTextureCtrl->setEnabled(editable);
					mTextureCtrl->setImageAssetID(id);
					getChildView("combobox alphamode")->setEnabled(editable && mIsAlpha && transparency <= 0.f);
					getChildView("label alphamode")->setEnabled(editable && mIsAlpha);
					getChildView("maskcutoff")->setEnabled(editable && mIsAlpha);
					getChildView("label maskcutoff")->setEnabled(editable && mIsAlpha);
					
					mTextureCtrl->setBakeTextureEnabled(TRUE);
				}
				
			}

			if (mShinyTextureCtrl)
			{
				mShinyTextureCtrl->setTentative( !identical_spec );
				mShinyTextureCtrl->setEnabled( editable );
				mShinyTextureCtrl->setImageAssetID( specmap_id );
			}

			if (mBumpyTextureCtrl)
			{
				mBumpyTextureCtrl->setTentative( !identical_norm );
				mBumpyTextureCtrl->setEnabled( editable );
				mBumpyTextureCtrl->setImageAssetID( normmap_id );
			}
		}

		// planar align
		bool align_planar = false;
		bool identical_planar_aligned = false;
		{
			LLCheckBoxCtrl*	cb_planar_align = getChild<LLCheckBoxCtrl>("checkbox planar align");
			align_planar = (cb_planar_align && cb_planar_align->get());

			bool enabled = (editable && isIdenticalPlanarTexgen());
			childSetValue("checkbox planar align", align_planar && enabled);
			childSetVisible("checkbox planar align", enabled);
			childSetEnabled("checkbox planar align", enabled);
			childSetEnabled("button align textures", enabled && LLSelectMgr::getInstance()->getSelection()->getObjectCount() > 1);

			if (align_planar && enabled)
			{
				LLFace* last_face = NULL;
				bool identical_face = false;
				LLSelectedTE::getFace(last_face, identical_face);

				LLPanelFaceGetIsAlignedTEFunctor get_is_aligend_func(last_face);
				// this will determine if the texture param controls are tentative:
				identical_planar_aligned = LLSelectMgr::getInstance()->getSelection()->applyToTEs(&get_is_aligend_func);
			}
		}
		
		// Needs to be public and before tex scale settings below to properly reflect
		// behavior when in planar vs default texgen modes in the
		// NORSPEC-84 et al
		//
		LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
		bool identical_texgen = true;		
		bool identical_planar_texgen = false;

		{	
			LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
			identical_planar_texgen = (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
		}

		// Texture scale
		{
			bool identical_diff_scale_s = false;
			bool identical_spec_scale_s = false;
			bool identical_norm_scale_s = false;

			identical = align_planar ? identical_planar_aligned : identical;

			F32 diff_scale_s = 1.f;			
			F32 spec_scale_s = 1.f;
			F32 norm_scale_s = 1.f;

			LLSelectedTE::getScaleS(diff_scale_s, identical_diff_scale_s);			
			LLSelectedTEMaterial::getSpecularRepeatX(spec_scale_s, identical_spec_scale_s);
			LLSelectedTEMaterial::getNormalRepeatX(norm_scale_s, identical_norm_scale_s);

			diff_scale_s = editable ? diff_scale_s : 1.0f;
			diff_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;
			
			norm_scale_s = editable ? norm_scale_s : 1.0f;
			norm_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

			spec_scale_s = editable ? spec_scale_s : 1.0f;
			spec_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

			mCtrlTexScaleU->setValue(diff_scale_s);
			mCtrlShinyScaleU->setValue(spec_scale_s);
			mCtrlBumpyScaleU->setValue(norm_scale_s);

			mCtrlTexScaleU->setEnabled(editable);
			mCtrlShinyScaleU->setEnabled(editable && specmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment
			mCtrlBumpyScaleU->setEnabled(editable && normmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment

			BOOL diff_scale_tentative = !(identical && identical_diff_scale_s);
			BOOL norm_scale_tentative = !(identical && identical_norm_scale_s);
			BOOL spec_scale_tentative = !(identical && identical_spec_scale_s);

			mCtrlTexScaleU->setTentative(  LLSD(diff_scale_tentative));
			mCtrlShinyScaleU->setTentative(LLSD(spec_scale_tentative));
			mCtrlBumpyScaleU->setTentative(LLSD(norm_scale_tentative));
			
			// <FS:CR> FIRE-11407 - Materials alignment
			getChildView("checkbox_sync_settings")->setEnabled(editable && (specmap_id.notNull() || normmap_id.notNull()) && !align_planar);
		}

		{
			bool identical_diff_scale_t = false;
			bool identical_spec_scale_t = false;
			bool identical_norm_scale_t = false;

			F32 diff_scale_t = 1.f;			
			F32 spec_scale_t = 1.f;
			F32 norm_scale_t = 1.f;

			LLSelectedTE::getScaleT(diff_scale_t, identical_diff_scale_t);
			LLSelectedTEMaterial::getSpecularRepeatY(spec_scale_t, identical_spec_scale_t);
			LLSelectedTEMaterial::getNormalRepeatY(norm_scale_t, identical_norm_scale_t);

			diff_scale_t = editable ? diff_scale_t : 1.0f;
			diff_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			norm_scale_t = editable ? norm_scale_t : 1.0f;
			norm_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			spec_scale_t = editable ? spec_scale_t : 1.0f;
			spec_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

			BOOL diff_scale_tentative = !identical_diff_scale_t;
			BOOL norm_scale_tentative = !identical_norm_scale_t;
			BOOL spec_scale_tentative = !identical_spec_scale_t;

			mCtrlTexScaleV->setEnabled(editable);
			mCtrlShinyScaleV->setEnabled(editable && specmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment
			mCtrlBumpyScaleV->setEnabled(editable && normmap_id.notNull()
										 && enable_material_controls); // <FS:CR> Materials alignment

			if (force_set_values)
			{
				mCtrlTexScaleV->forceSetValue(diff_scale_t);
			}
			else
			{
				mCtrlTexScaleV->setValue(diff_scale_t);
			}
			mCtrlShinyScaleV->setValue(norm_scale_t);
			mCtrlBumpyScaleV->setValue(spec_scale_t);

			mCtrlTexScaleV->setTentative(LLSD(diff_scale_tentative));
			mCtrlShinyScaleV->setTentative(LLSD(norm_scale_tentative));
			mCtrlBumpyScaleV->setTentative(LLSD(spec_scale_tentative));
		}

		// Texture offset
		{
			bool identical_diff_offset_s = false;
			bool identical_norm_offset_s = false;
			bool identical_spec_offset_s = false;

			F32 diff_offset_s = 0.0f;
			F32 norm_offset_s = 0.0f;
			F32 spec_offset_s = 0.0f;

			LLSelectedTE::getOffsetS(diff_offset_s, identical_diff_offset_s);
			LLSelectedTEMaterial::getNormalOffsetX(norm_offset_s, identical_norm_offset_s);
			LLSelectedTEMaterial::getSpecularOffsetX(spec_offset_s, identical_spec_offset_s);

			BOOL diff_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_s);
			BOOL norm_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_s);
			BOOL spec_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_s);

			mCtrlTexOffsetU->setValue(  editable ? diff_offset_s : 0.0f);
			mCtrlBumpyOffsetU->setValue(editable ? norm_offset_s : 0.0f);
			mCtrlShinyOffsetU->setValue(editable ? spec_offset_s : 0.0f);

			mCtrlTexOffsetU->setTentative(LLSD(diff_offset_u_tentative));
			mCtrlBumpyOffsetU->setTentative(LLSD(norm_offset_u_tentative));
			mCtrlShinyOffsetU->setTentative(LLSD(spec_offset_u_tentative));

			mCtrlTexOffsetU->setEnabled(editable);
			mCtrlShinyOffsetU->setEnabled(editable && specmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
			mCtrlBumpyOffsetU->setEnabled(editable && normmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
		}

		{
			bool identical_diff_offset_t = false;
			bool identical_norm_offset_t = false;
			bool identical_spec_offset_t = false;

			F32 diff_offset_t = 0.0f;
			F32 norm_offset_t = 0.0f;
			F32 spec_offset_t = 0.0f;

			LLSelectedTE::getOffsetT(diff_offset_t, identical_diff_offset_t);
			LLSelectedTEMaterial::getNormalOffsetY(norm_offset_t, identical_norm_offset_t);
			LLSelectedTEMaterial::getSpecularOffsetY(spec_offset_t, identical_spec_offset_t);
			
			BOOL diff_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_t);
			BOOL norm_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_t);
			BOOL spec_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_t);

			mCtrlTexOffsetV->setValue(  editable ? diff_offset_t : 0.0f);
			mCtrlBumpyOffsetV->setValue(editable ? norm_offset_t : 0.0f);
			mCtrlShinyOffsetV->setValue(editable ? spec_offset_t : 0.0f);

			mCtrlTexOffsetV->setTentative(LLSD(diff_offset_v_tentative));
			mCtrlBumpyOffsetV->setTentative(LLSD(norm_offset_v_tentative));
			mCtrlShinyOffsetV->setTentative(LLSD(spec_offset_v_tentative));

			mCtrlTexOffsetV->setEnabled(editable);
			mCtrlShinyOffsetV->setEnabled(editable && specmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
			mCtrlBumpyOffsetV->setEnabled(editable && normmap_id.notNull()
										  && enable_material_controls); // <FS:CR> Materials alignment
		}

		// Texture rotation
		{
			bool identical_diff_rotation = false;
			bool identical_norm_rotation = false;
			bool identical_spec_rotation = false;

			F32 diff_rotation = 0.f;
			F32 norm_rotation = 0.f;
			F32 spec_rotation = 0.f;

			LLSelectedTE::getRotation(diff_rotation,identical_diff_rotation);
			LLSelectedTEMaterial::getSpecularRotation(spec_rotation,identical_spec_rotation);
			LLSelectedTEMaterial::getNormalRotation(norm_rotation,identical_norm_rotation);

			BOOL diff_rot_tentative = !(align_planar ? identical_planar_aligned : identical_diff_rotation);
			BOOL norm_rot_tentative = !(align_planar ? identical_planar_aligned : identical_norm_rotation);
			BOOL spec_rot_tentative = !(align_planar ? identical_planar_aligned : identical_spec_rotation);

			F32 diff_rot_deg = diff_rotation * RAD_TO_DEG;
			F32 norm_rot_deg = norm_rotation * RAD_TO_DEG;
			F32 spec_rot_deg = spec_rotation * RAD_TO_DEG;
			
			mCtrlTexRot->setEnabled(editable);
			mCtrlShinyRot->setEnabled(editable && specmap_id.notNull()
									  && enable_material_controls); // <FS:CR> Materials alignment
			mCtrlBumpyRot->setEnabled(editable && normmap_id.notNull()
									  && enable_material_controls); // <FS:CR> Materials alignment

			mCtrlTexRot->setTentative(diff_rot_tentative);
			mCtrlBumpyRot->setTentative(LLSD(norm_rot_tentative));
			mCtrlShinyRot->setTentative(LLSD(spec_rot_tentative));

			mCtrlTexRot->setValue(  editable ? diff_rot_deg : 0.0f);			
			mCtrlShinyRot->setValue(editable ? spec_rot_deg : 0.0f);
			mCtrlBumpyRot->setValue(editable ? norm_rot_deg : 0.0f);
		}

		{
			F32 glow = 0.f;
			bool identical_glow = false;
			LLSelectedTE::getGlow(glow,identical_glow);
			LLUICtrl* glow_ctrl = getChild<LLUICtrl>("glow");
			glow_ctrl->setValue(glow);
			glow_ctrl->setTentative(!identical_glow);
			glow_ctrl->setEnabled(editable);
			getChildView("glow label")->setEnabled(editable);
		}

		{
			LLCtrlSelectionInterface* combobox_texgen = childGetSelectionInterface("combobox texgen");
			if (combobox_texgen)
			{
				// Maps from enum to combobox entry index
				combobox_texgen->selectNthItem(((S32)selected_texgen) >> 1);
			}
			else
				{
				LL_WARNS() << "failed childGetSelectionInterface for 'combobox texgen'" << LL_ENDL;
				}

			getChildView("combobox texgen")->setEnabled(editable);
			getChild<LLUICtrl>("combobox texgen")->setTentative(!identical);
			getChildView("tex gen")->setEnabled(editable);

			}

		{
			U8 fullbright_flag = 0;
			bool identical_fullbright = false;
			
			LLSelectedTE::getFullbright(fullbright_flag,identical_fullbright);

			LLUICtrl* check_fullbright = getChild<LLUICtrl>("checkbox fullbright");
			check_fullbright->setValue((S32)(fullbright_flag != 0));
			check_fullbright->setEnabled(editable);
			check_fullbright->setTentative(!identical_fullbright);
		}
		
		// Repeats per meter
		{
			F32 repeats_diff = 1.f;
			F32 repeats_norm = 1.f;
			F32 repeats_spec = 1.f;

			bool identical_diff_repeats = false;
			bool identical_norm_repeats = false;
			bool identical_spec_repeats = false;

			LLSelectedTE::getMaxDiffuseRepeats(repeats_diff, identical_diff_repeats);
			LLSelectedTEMaterial::getMaxNormalRepeats(repeats_norm, identical_norm_repeats);
			LLSelectedTEMaterial::getMaxSpecularRepeats(repeats_spec, identical_spec_repeats);			

			LLComboBox*	mComboTexGen = getChild<LLComboBox>("combobox texgen");
			if (mComboTexGen)
		{
				S32 index = mComboTexGen ? mComboTexGen->getCurrentIndex() : 0;
				BOOL enabled = editable && (index != 1);
				BOOL identical_repeats = true;
				F32  repeats = 1.0f;

				U32 material_type = (mComboMatMedia->getCurrentIndex() == MATMEDIA_MATERIAL) ? mRadioMatType->getSelectedIndex() : MATTYPE_DIFFUSE;
				LLSelectMgr::getInstance()->setTextureChannel(LLRender::eTexIndex(material_type));

				switch (material_type)
			{
					default:
					case MATTYPE_DIFFUSE:
				{
						enabled = editable && !id.isNull();
						identical_repeats = identical_diff_repeats;
						repeats = repeats_diff;
				}
					break;

					case MATTYPE_SPECULAR:
			{
						enabled = (editable && ((shiny == SHINY_TEXTURE) && !specmap_id.isNull())
								   && enable_material_controls);	// <FS:CR> Materials Alignment
						identical_repeats = identical_spec_repeats;
						repeats = repeats_spec;
			}
					break;

					case MATTYPE_NORMAL:
			{
						enabled = (editable && ((bumpy == BUMPY_TEXTURE) && !normmap_id.isNull())
								   && enable_material_controls); // <FS:CR> Materials Alignment
						identical_repeats = identical_norm_repeats;
						repeats = repeats_norm;
					}
					break;
				}

				BOOL repeats_tentative = !identical_repeats;

				mCtrlRpt->setEnabled(identical_planar_texgen ? FALSE : enabled);
				//LLSpinCtrl* rpt_ctrl = getChild<LLSpinCtrl>("rptctrl");
				if (force_set_values)
				{
					//onCommit, previosly edited element updates related ones
					mCtrlRpt->forceSetValue(editable ? repeats : 1.0f);
				}
				else
				{
					mCtrlRpt->setValue(editable ? repeats : 1.0f);
				}
				mCtrlRpt->setTentative(LLSD(repeats_tentative));

				// <FS:CR> FIRE-11407 - Flip buttons
				getChildView("flipTextureScaleU")->setEnabled(enabled);
				getChildView("flipTextureScaleV")->setEnabled(enabled);
				// </FS:CR>
			}
		}

		// Materials
		{
			LLMaterialPtr material;
			LLSelectedTEMaterial::getCurrent(material, identical);

			if (material && editable)
			{
				LL_DEBUGS("Materials") << material->asLLSD() << LL_ENDL;

				// Alpha
				LLCtrlSelectionInterface* combobox_alphamode =
					childGetSelectionInterface("combobox alphamode");
				if (combobox_alphamode)
				{
					U32 alpha_mode = material->getDiffuseAlphaMode();

					if (transparency > 0.f)
					{ //it is invalid to have any alpha mode other than blend if transparency is greater than zero ... 
						alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
					}

					if (!mIsAlpha)
					{ // ... unless there is no alpha channel in the texture, in which case alpha mode MUST ebe none
						alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
				}

					combobox_alphamode->selectNthItem(alpha_mode);
			}
			else
			{
					LL_WARNS() << "failed childGetSelectionInterface for 'combobox alphamode'" << LL_ENDL;
			}
				getChild<LLUICtrl>("maskcutoff")->setValue(material->getAlphaMaskCutoff());
				updateAlphaControls();

				identical_planar_texgen = isIdenticalPlanarTexgen();

				// Shiny (specular)
				F32 offset_x, offset_y, repeat_x, repeat_y, rot;
				//LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>("shinytexture control");
				mShinyTextureCtrl->setImageAssetID(material->getSpecularID());

				if (!material->getSpecularID().isNull() && (shiny == SHINY_TEXTURE))
			{
					material->getSpecularOffset(offset_x,offset_y);
					material->getSpecularRepeat(repeat_x,repeat_y);

					if (identical_planar_texgen)
			{
						repeat_x *= 2.0f;
						repeat_y *= 2.0f;
			}

					rot = material->getSpecularRotation();
					mCtrlShinyScaleU->setValue(repeat_x);
					mCtrlShinyScaleV->setValue(repeat_y);
					mCtrlShinyRot->setValue(rot*RAD_TO_DEG);
					mCtrlShinyOffsetU->setValue(offset_x);
					mCtrlShinyOffsetV->setValue(offset_y);
					getChild<LLUICtrl>("glossiness")->setValue(material->getSpecularLightExponent());
					getChild<LLUICtrl>("environment")->setValue(material->getEnvironmentIntensity());

					updateShinyControls(!material->getSpecularID().isNull(), true);
		}

				// Assert desired colorswatch color to match material AFTER updateShinyControls
				// to avoid getting overwritten with the default on some UI state changes.
				//
				if (!material->getSpecularID().isNull())
				{
					LLColorSwatchCtrl*	shiny_swatch = getChild<LLColorSwatchCtrl>("shinycolorswatch");
					LLColor4 new_color = material->getSpecularLightColor();
					LLColor4 old_color = shiny_swatch->get();

					shiny_swatch->setOriginal(new_color);
					shiny_swatch->set(new_color, force_set_values || old_color != new_color || !editable);
				}

				// Bumpy (normal)
				mBumpyTextureCtrl->setImageAssetID(material->getNormalID());

				if (!material->getNormalID().isNull())
				{
					material->getNormalOffset(offset_x,offset_y);
					material->getNormalRepeat(repeat_x,repeat_y);

					if (identical_planar_texgen)
					{
						repeat_x *= 2.0f;
						repeat_y *= 2.0f;
					}
			
					rot = material->getNormalRotation();
					mCtrlBumpyScaleU->setValue(repeat_x);
					mCtrlBumpyScaleV->setValue(repeat_y);
					mCtrlBumpyRot->setValue(rot*RAD_TO_DEG);
					mCtrlBumpyOffsetU->setValue(offset_x);
					mCtrlBumpyOffsetV->setValue(offset_y);

					updateBumpyControls(!material->getNormalID().isNull(), true);
				}
			}
		}
        S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
        BOOL single_volume = (selected_count == 1);
        // <FS> Extended copy & paste buttons
        //mMenuClipboardColor->setEnabled(editable && single_volume);
        mBtnCopyFaces->setEnabled(editable && single_volume);
        mBtnPasteFaces->setEnabled(editable && !mClipboardParams.emptyMap() && (mClipboardParams.has("color") || mClipboardParams.has("texture")));

		LL_INFOS() << "ADBG: enabled = " << (editable && !mClipboardParams.emptyMap() && (mClipboardParams.has("color") || mClipboardParams.has("texture")) ? "true" : "false") << LL_ENDL;
        // </FS>

		// Set variable values for numeric expressions
		LLCalc* calcp = LLCalc::getInstance();
		calcp->setVar(LLCalc::TEX_U_SCALE, getCurrentTextureScaleU());
		calcp->setVar(LLCalc::TEX_V_SCALE, getCurrentTextureScaleV());
		calcp->setVar(LLCalc::TEX_U_OFFSET, getCurrentTextureOffsetU());
		calcp->setVar(LLCalc::TEX_V_OFFSET, getCurrentTextureOffsetV());
		calcp->setVar(LLCalc::TEX_ROTATION, getCurrentTextureRot());
		calcp->setVar(LLCalc::TEX_TRANSPARENCY, childGetValue("ColorTrans").asReal());
		calcp->setVar(LLCalc::TEX_GLOW, childGetValue("glow").asReal());
	}
	else
	{
		// Disable all UICtrls
		clearCtrls();

		// Disable non-UICtrls
		//LLTextureCtrl*	texture_ctrl = getChild<LLTextureCtrl>("texture control");
		if(mTextureCtrl)
		{
			mTextureCtrl->setImageAssetID( LLUUID::null );
			mTextureCtrl->setEnabled( FALSE );  // this is a LLUICtrl, but we don't want it to have keyboard focus so we add it as a child, not a ctrl.
// 			texture_ctrl->setValid(FALSE);
		}
		//LLColorSwatchCtrl* mColorSwatch = getChild<LLColorSwatchCtrl>("colorswatch");
		if(mColorSwatch)
		{
			mColorSwatch->setEnabled( FALSE );			
			mColorSwatch->setFallbackImage(LLUI::getUIImage("locked_image.j2c") );
			mColorSwatch->setValid(FALSE);
		}
		LLRadioGroup* radio_mat_type = getChild<LLRadioGroup>("radio_material_type");
		if (radio_mat_type)
		{
			radio_mat_type->setSelectedIndex(0);
		}
		getChildView("color trans")->setEnabled(FALSE);
		mCtrlRpt->setEnabled(FALSE);
		getChildView("tex gen")->setEnabled(FALSE);
		getChildView("label shininess")->setEnabled(FALSE);
		getChildView("label bumpiness")->setEnabled(FALSE);
		getChildView("button align")->setEnabled(FALSE);
		//getChildView("has media")->setEnabled(FALSE);
		//getChildView("media info set")->setEnabled(FALSE);

		// <FS> Extended copy & paste buttons
		mBtnCopyFaces->setEnabled(FALSE);
		mBtnPasteFaces->setEnabled(FALSE);
		// </FS>
		
		updateVisibility();

		// Set variable values for numeric expressions
		LLCalc* calcp = LLCalc::getInstance();
		calcp->clearVar(LLCalc::TEX_U_SCALE);
		calcp->clearVar(LLCalc::TEX_V_SCALE);
		calcp->clearVar(LLCalc::TEX_U_OFFSET);
		calcp->clearVar(LLCalc::TEX_V_OFFSET);
		calcp->clearVar(LLCalc::TEX_ROTATION);
		calcp->clearVar(LLCalc::TEX_TRANSPARENCY);
		calcp->clearVar(LLCalc::TEX_GLOW);		
	}
}


void LLPanelFace::updateCopyTexButton()
{
    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
    // <FS> Extended copy & paste buttons
    //mMenuClipboardTexture->setEnabled(objectp && objectp->getPCode() == LL_PCODE_VOLUME && objectp->permModify() 
    //                                                && !objectp->isPermanentEnforced() && !objectp->isInventoryPending() 
    //                                                && (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1));
    //std::string tooltip = (objectp && objectp->isInventoryPending()) ? LLTrans::getString("LoadingContents") : getString("paste_options");
    //mMenuClipboardTexture->setToolTip(tooltip);
    mBtnCopyFaces->setEnabled(objectp && objectp->getPCode() == LL_PCODE_VOLUME && objectp->permModify()
        && !objectp->isPermanentEnforced() && !objectp->isInventoryPending()
        && (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1));
    std::string tooltip = (objectp && objectp->isInventoryPending()) ? LLTrans::getString("LoadingContents") : getString("paste_options");
    mBtnCopyFaces->setToolTip(tooltip);
    // </FS>
}

void LLPanelFace::refresh()
{
	LL_DEBUGS("Materials") << LL_ENDL;
	getState();
}

//
// Static functions
//

// static
F32 LLPanelFace::valueGlow(LLViewerObject* object, S32 face)
{
	return (F32)(object->getTEref(face).getGlow());
}


void LLPanelFace::onCommitColor(const LLSD& data)
{
	sendColor();
}

void LLPanelFace::onCommitShinyColor(const LLSD& data)
{
	LLSelectedTEMaterial::setSpecularLightColor(this, getChild<LLColorSwatchCtrl>("shinycolorswatch")->get());
}

void LLPanelFace::onCommitAlpha(const LLSD& data)
{
	sendAlpha();
}

void LLPanelFace::onCancelColor(const LLSD& data)
{
	LLSelectMgr::getInstance()->selectionRevertColors();
}

void LLPanelFace::onCancelShinyColor(const LLSD& data)
{
	LLSelectMgr::getInstance()->selectionRevertShinyColors();
}

void LLPanelFace::onSelectColor(const LLSD& data)
{
	LLSelectMgr::getInstance()->saveSelectedObjectColors();
	sendColor();
}

void LLPanelFace::onSelectShinyColor(const LLSD& data)
{
	LLSelectedTEMaterial::setSpecularLightColor(this, getChild<LLColorSwatchCtrl>("shinycolorswatch")->get());
	LLSelectMgr::getInstance()->saveSelectedShinyColors();
}

// static
void LLPanelFace::onCommitMaterialsMedia(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	// Force to default states to side-step problems with menu contents
	// and generally reflecting old state when switching tabs or objects
	//
	self->updateShinyControls(false,true);
	self->updateBumpyControls(false,true);
	self->updateUI();
}

// static
void LLPanelFace::updateVisibility()
{	
	LLComboBox* combo_matmedia = getChild<LLComboBox>("combobox matmedia");
	LLRadioGroup* radio_mat_type = getChild<LLRadioGroup>("radio_material_type");
	LLComboBox* combo_shininess = getChild<LLComboBox>("combobox shininess");
	LLComboBox* combo_bumpiness = getChild<LLComboBox>("combobox bumpiness");
	if (!radio_mat_type || !combo_matmedia || !combo_shininess || !combo_bumpiness)
	{
		LL_WARNS("Materials") << "Combo box not found...exiting." << LL_ENDL;
		return;
	}
	U32 materials_media = combo_matmedia->getCurrentIndex();
	U32 material_type = radio_mat_type->getSelectedIndex();
	bool show_media = (materials_media == MATMEDIA_MEDIA) && combo_matmedia->getEnabled();
	bool show_texture = (show_media || ((material_type == MATTYPE_DIFFUSE) && combo_matmedia->getEnabled()));
	bool show_bumpiness = (!show_media) && (material_type == MATTYPE_NORMAL) && combo_matmedia->getEnabled();
	bool show_shininess = (!show_media) && (material_type == MATTYPE_SPECULAR) && combo_matmedia->getEnabled();
	getChildView("radio_material_type")->setVisible(!show_media);
	// <FS:CR> FIRE-11407 - Be consistant and hide this with the other controls
	getChildView("rptctrl")->setVisible(combo_matmedia->getEnabled());
	getChildView("tex gen")->setVisible(combo_matmedia->getEnabled());
	getChildView("combobox texgen")->setVisible(combo_matmedia->getEnabled());
	getChildView("checkbox planar align")->setVisible(combo_matmedia->getEnabled());
	getChildView("checkbox_sync_settings")->setVisible(combo_matmedia->getEnabled());
	getChildView("button align textures")->setVisible(combo_matmedia->getEnabled());
	// and other additions...
	getChildView("flipTextureScaleU")->setVisible(combo_matmedia->getEnabled());
	getChildView("flipTextureScaleV")->setVisible(combo_matmedia->getEnabled());
	// </FS:CR>

	// Media controls
	getChildView("media_info")->setVisible(show_media);
	getChildView("add_media")->setVisible(show_media);
	getChildView("delete_media")->setVisible(show_media);
	getChildView("button align")->setVisible(show_media);

	// Diffuse texture controls
	getChildView("texture control")->setVisible(show_texture && !show_media);
	getChildView("label alphamode")->setVisible(show_texture && !show_media);
	getChildView("combobox alphamode")->setVisible(show_texture && !show_media);
	getChildView("label maskcutoff")->setVisible(false);
	getChildView("maskcutoff")->setVisible(false);
	if (show_texture && !show_media)
	{
		updateAlphaControls();
	}
	getChildView("TexScaleU")->setVisible(show_texture);
	getChildView("TexScaleV")->setVisible(show_texture);
	getChildView("TexRot")->setVisible(show_texture);
	getChildView("TexOffsetU")->setVisible(show_texture);
	getChildView("TexOffsetV")->setVisible(show_texture);

	// Specular map controls
	getChildView("shinytexture control")->setVisible(show_shininess);
	getChildView("combobox shininess")->setVisible(show_shininess);
	getChildView("label shininess")->setVisible(show_shininess);
	getChildView("label glossiness")->setVisible(false);
	getChildView("glossiness")->setVisible(false);
	getChildView("label environment")->setVisible(false);
	getChildView("environment")->setVisible(false);
	getChildView("label shinycolor")->setVisible(false);
	getChildView("shinycolorswatch")->setVisible(false);
	if (show_shininess)
	{
		updateShinyControls();
	}
	getChildView("shinyScaleU")->setVisible(show_shininess);
	getChildView("shinyScaleV")->setVisible(show_shininess);
	getChildView("shinyRot")->setVisible(show_shininess);
	getChildView("shinyOffsetU")->setVisible(show_shininess);
	getChildView("shinyOffsetV")->setVisible(show_shininess);

	// Normal map controls
	if (show_bumpiness)
	{
		updateBumpyControls();
	}
	getChildView("bumpytexture control")->setVisible(show_bumpiness);
	getChildView("combobox bumpiness")->setVisible(show_bumpiness);
	getChildView("label bumpiness")->setVisible(show_bumpiness);
	getChildView("bumpyScaleU")->setVisible(show_bumpiness);
	getChildView("bumpyScaleV")->setVisible(show_bumpiness);
	getChildView("bumpyRot")->setVisible(show_bumpiness);
	getChildView("bumpyOffsetU")->setVisible(show_bumpiness);
	getChildView("bumpyOffsetV")->setVisible(show_bumpiness);


}

// static
void LLPanelFace::onCommitMaterialType(LLUICtrl* ctrl, void* userdata)
{
    LLPanelFace* self = (LLPanelFace*) userdata;
	 // Force to default states to side-step problems with menu contents
	 // and generally reflecting old state when switching tabs or objects
	 //
	 self->updateShinyControls(false,true);
	 self->updateBumpyControls(false,true);
    self->updateUI();
}

// static
void LLPanelFace::onCommitBump(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;

	LLComboBox*	mComboBumpiness = self->getChild<LLComboBox>("combobox bumpiness");
	if(!mComboBumpiness)
		return;

	U32 bumpiness = mComboBumpiness->getCurrentIndex();

	self->sendBump(bumpiness);
}

// static
void LLPanelFace::onCommitTexGen(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	self->sendTexGen();
}

// static
void LLPanelFace::updateShinyControls(bool is_setting_texture, bool mess_with_shiny_combobox)
{
	//LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>("shinytexture control");
	LLUUID shiny_texture_ID = mShinyTextureCtrl->getImageAssetID();
	LL_DEBUGS("Materials") << "Shiny texture selected: " << shiny_texture_ID << LL_ENDL;
	LLComboBox* comboShiny = getChild<LLComboBox>("combobox shininess");

	if(mess_with_shiny_combobox)
	{
		if (!comboShiny)
		{
			return;
		}
		if (!shiny_texture_ID.isNull() && is_setting_texture)
		{
			if (!comboShiny->itemExists(USE_TEXTURE))
			{
				comboShiny->add(USE_TEXTURE);
			}
			comboShiny->setSimple(USE_TEXTURE);
		}
		else
		{
			if (comboShiny->itemExists(USE_TEXTURE))
			{
				comboShiny->remove(SHINY_TEXTURE);
				comboShiny->selectFirstItem();
			}
		}
	}
	else
	{
		if (shiny_texture_ID.isNull() && comboShiny && comboShiny->itemExists(USE_TEXTURE))
		{
			comboShiny->remove(SHINY_TEXTURE);
			comboShiny->selectFirstItem();
		}
	}


	LLComboBox* combo_matmedia = getChild<LLComboBox>("combobox matmedia");
	LLRadioGroup* radio_mat_type = getChild<LLRadioGroup>("radio_material_type");
	U32 materials_media = combo_matmedia->getCurrentIndex();
	U32 material_type = radio_mat_type->getSelectedIndex();
	bool show_media = (materials_media == MATMEDIA_MEDIA) && combo_matmedia->getEnabled();
	bool show_shininess = (!show_media) && (material_type == MATTYPE_SPECULAR) && combo_matmedia->getEnabled();
	U32 shiny_value = comboShiny->getCurrentIndex();
	bool show_shinyctrls = (shiny_value == SHINY_TEXTURE) && show_shininess; // Use texture
	getChildView("label glossiness")->setVisible(show_shinyctrls);
	getChildView("glossiness")->setVisible(show_shinyctrls);
	getChildView("label environment")->setVisible(show_shinyctrls);
	getChildView("environment")->setVisible(show_shinyctrls);
	getChildView("label shinycolor")->setVisible(show_shinyctrls);
	getChildView("shinycolorswatch")->setVisible(show_shinyctrls);
}

// static
void LLPanelFace::updateBumpyControls(bool is_setting_texture, bool mess_with_combobox)
{
	//LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>("bumpytexture control");
	LLUUID bumpy_texture_ID = mBumpyTextureCtrl->getImageAssetID();
	LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;
	LLComboBox* comboBumpy = getChild<LLComboBox>("combobox bumpiness");
	if (!comboBumpy)
	{
		return;
	}

	if (mess_with_combobox)
	{
		LLUUID bumpy_texture_ID = mBumpyTextureCtrl->getImageAssetID();
		LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;

		if (!bumpy_texture_ID.isNull() && is_setting_texture)
		{
			if (!comboBumpy->itemExists(USE_TEXTURE))
			{
				comboBumpy->add(USE_TEXTURE);
			}
			comboBumpy->setSimple(USE_TEXTURE);
		}
		else
		{
			if (comboBumpy->itemExists(USE_TEXTURE))
			{
				comboBumpy->remove(BUMPY_TEXTURE);
				comboBumpy->selectFirstItem();
			}
		}
	}
}

// static
void LLPanelFace::onCommitShiny(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	LLComboBox*	combo_shininess = self->getChild<LLComboBox>("combobox shininess");
	if(!combo_shininess)
		return;
	
	U32 shininess = combo_shininess->getCurrentIndex();

	self->sendShiny(shininess);
}

// static
void LLPanelFace::updateAlphaControls()
{
	LLComboBox* comboAlphaMode = getChild<LLComboBox>("combobox alphamode");
	if (!comboAlphaMode)
	{
		return;
	}
	U32 alpha_value = comboAlphaMode->getCurrentIndex();
	bool show_alphactrls = (alpha_value == ALPHAMODE_MASK); // Alpha masking
    
    //LLComboBox* combobox_matmedia = getChild<LLComboBox>("combobox matmedia");
    U32 mat_media = MATMEDIA_MATERIAL;
    if (mComboMatMedia)
    {
        mat_media = mComboMatMedia->getCurrentIndex();
    }
    
    U32 mat_type = MATTYPE_DIFFUSE;
    //LLRadioGroup* radio_mat_type = getChild<LLRadioGroup>("radio_material_type");
    if(mRadioMatType)
    {
        mat_type = mRadioMatType->getSelectedIndex();
    }

    show_alphactrls = show_alphactrls && (mat_media == MATMEDIA_MATERIAL);
    show_alphactrls = show_alphactrls && (mat_type == MATTYPE_DIFFUSE);
    
	getChildView("label maskcutoff")->setVisible(show_alphactrls);
	getChildView("maskcutoff")->setVisible(show_alphactrls);
}

// static
void LLPanelFace::onCommitAlphaMode(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	self->updateAlphaControls();
	LLSelectedTEMaterial::setDiffuseAlphaMode(self,self->getCurrentDiffuseAlphaMode());
}

// static
void LLPanelFace::onCommitFullbright(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	self->sendFullbright();
}

// static
void LLPanelFace::onCommitGlow(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	self->sendGlow();
}

// static
BOOL LLPanelFace::onDragTexture(LLUICtrl*, LLInventoryItem* item)
{
	BOOL accept = TRUE;
	for (LLObjectSelection::root_iterator iter = LLSelectMgr::getInstance()->getSelection()->root_begin();
		 iter != LLSelectMgr::getInstance()->getSelection()->root_end(); iter++)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* obj = node->getObject();
		if(!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
		{
			accept = FALSE;
			break;
		}
	}
	return accept;
}

void LLPanelFace::onCommitTexture( const LLSD& data )
{
	add(LLStatViewer::EDIT_TEXTURE, 1);
	sendTexture();
}

void LLPanelFace::onCancelTexture(const LLSD& data)
{
	LLSelectMgr::getInstance()->selectionRevertTextures();
}

void LLPanelFace::onSelectTexture(const LLSD& data)
{
	LLSelectMgr::getInstance()->saveSelectedObjectTextures();
	sendTexture();

	LLGLenum image_format;
	bool identical_image_format = false;
	LLSelectedTE::getImageFormat(image_format, identical_image_format);
    
	LLCtrlSelectionInterface* combobox_alphamode =
		childGetSelectionInterface("combobox alphamode");

	U32 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
	if (combobox_alphamode)
	{
		switch (image_format)
		{
		case GL_RGBA:
		case GL_ALPHA:
			{
				alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
			}
			break;

		case GL_RGB: break;
		default:
			{
				LL_WARNS() << "Unexpected tex format in LLPanelFace...resorting to no alpha" << LL_ENDL;
			}
			break;
		}

		combobox_alphamode->selectNthItem(alpha_mode);
	}
	LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());
}

void LLPanelFace::onCloseTexturePicker(const LLSD& data)
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	updateUI();
}

void LLPanelFace::onCommitSpecularTexture( const LLSD& data )
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	sendShiny(SHINY_TEXTURE);
}

void LLPanelFace::onCommitNormalTexture( const LLSD& data )
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	LLUUID nmap_id = getCurrentNormalMap();
	sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
}

void LLPanelFace::onCancelSpecularTexture(const LLSD& data)
{
	U8 shiny = 0;
	bool identical_shiny = false;
	LLSelectedTE::getShiny(shiny, identical_shiny);
	LLUUID spec_map_id = mShinyTextureCtrl->getImageAssetID();
	shiny = spec_map_id.isNull() ? shiny : SHINY_TEXTURE;
	sendShiny(shiny);
}

void LLPanelFace::onCancelNormalTexture(const LLSD& data)
{
	U8 bumpy = 0;
	bool identical_bumpy = false;
	LLSelectedTE::getBumpmap(bumpy, identical_bumpy);
	LLUUID spec_map_id = getChild<LLTextureCtrl>("bumpytexture control")->getImageAssetID();
	bumpy = spec_map_id.isNull() ? bumpy : BUMPY_TEXTURE;
	sendBump(bumpy);
}

void LLPanelFace::onSelectSpecularTexture(const LLSD& data)
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	sendShiny(SHINY_TEXTURE);
}

void LLPanelFace::onSelectNormalTexture(const LLSD& data)
{
	LL_DEBUGS("Materials") << data << LL_ENDL;
	LLUUID nmap_id = getCurrentNormalMap();
	sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
}

//static
void LLPanelFace::syncOffsetX(LLPanelFace* self, F32 offsetU)
{
	LLSelectedTEMaterial::setNormalOffsetX(self,offsetU);
	LLSelectedTEMaterial::setSpecularOffsetX(self,offsetU);
	self->getChild<LLSpinCtrl>("TexOffsetU")->forceSetValue(offsetU);
	self->sendTextureInfo();
}

//static
void LLPanelFace::syncOffsetY(LLPanelFace* self, F32 offsetV)
{
	LLSelectedTEMaterial::setNormalOffsetY(self,offsetV);
	LLSelectedTEMaterial::setSpecularOffsetY(self,offsetV);
	self->getChild<LLSpinCtrl>("TexOffsetV")->forceSetValue(offsetV);
	self->sendTextureInfo();
}

//static
void LLPanelFace::onCommitMaterialBumpyOffsetX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncOffsetX(self,self->getCurrentBumpyOffsetU());
	}
	else
	{
		LLSelectedTEMaterial::setNormalOffsetX(self,self->getCurrentBumpyOffsetU());
	}

}

//static
void LLPanelFace::onCommitMaterialBumpyOffsetY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncOffsetY(self,self->getCurrentBumpyOffsetV());
	}
	else
	{
		LLSelectedTEMaterial::setNormalOffsetY(self,self->getCurrentBumpyOffsetV());
	}
}

//static
void LLPanelFace::onCommitMaterialShinyOffsetX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncOffsetX(self, self->getCurrentShinyOffsetU());
	}
	else
	{
		LLSelectedTEMaterial::setSpecularOffsetX(self,self->getCurrentShinyOffsetU());
	}
}

//static
void LLPanelFace::onCommitMaterialShinyOffsetY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncOffsetY(self,self->getCurrentShinyOffsetV());
	}
	else
	{
		LLSelectedTEMaterial::setSpecularOffsetY(self,self->getCurrentShinyOffsetV());
	}
}

//static
void LLPanelFace::syncRepeatX(LLPanelFace* self, F32 scaleU)
{
	LLSelectedTEMaterial::setNormalRepeatX(self,scaleU);
	LLSelectedTEMaterial::setSpecularRepeatX(self,scaleU);
	self->sendTextureInfo();
}

//static
void LLPanelFace::syncRepeatY(LLPanelFace* self, F32 scaleV)
{
	LLSelectedTEMaterial::setNormalRepeatY(self,scaleV);
	LLSelectedTEMaterial::setSpecularRepeatY(self,scaleV);
	self->sendTextureInfo();
}

//static
void LLPanelFace::onCommitMaterialBumpyScaleX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 bumpy_scale_u = self->getCurrentBumpyScaleU();
	if (self->isIdenticalPlanarTexgen())
	{
		bumpy_scale_u *= 0.5f;
	}

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		self->getChild<LLSpinCtrl>("TexScaleU")->forceSetValue(self->getCurrentBumpyScaleU());
		syncRepeatX(self, bumpy_scale_u);
	}
	else
	{
		LLSelectedTEMaterial::setNormalRepeatX(self,bumpy_scale_u);
	}
}

//static
void LLPanelFace::onCommitMaterialBumpyScaleY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 bumpy_scale_v = self->getCurrentBumpyScaleV();
	if (self->isIdenticalPlanarTexgen())
	{
		bumpy_scale_v *= 0.5f;
	}


	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		self->getChild<LLSpinCtrl>("TexScaleV")->forceSetValue(self->getCurrentBumpyScaleV());
		syncRepeatY(self, bumpy_scale_v);
	}
	else
	{
		LLSelectedTEMaterial::setNormalRepeatY(self,bumpy_scale_v);
	}
}

//static
void LLPanelFace::onCommitMaterialShinyScaleX(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 shiny_scale_u = self->getCurrentShinyScaleU();
	if (self->isIdenticalPlanarTexgen())
	{
		shiny_scale_u *= 0.5f;
	}

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		self->getChild<LLSpinCtrl>("TexScaleU")->forceSetValue(self->getCurrentShinyScaleU());
		syncRepeatX(self, shiny_scale_u);
	}
	else
	{
		LLSelectedTEMaterial::setSpecularRepeatX(self,shiny_scale_u);
	}
}

//static
void LLPanelFace::onCommitMaterialShinyScaleY(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	F32 shiny_scale_v = self->getCurrentShinyScaleV();
	if (self->isIdenticalPlanarTexgen())
	{
		shiny_scale_v *= 0.5f;
	}

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		self->getChild<LLSpinCtrl>("TexScaleV")->forceSetValue(self->getCurrentShinyScaleV());
		syncRepeatY(self, shiny_scale_v);
	}
	else
	{
		LLSelectedTEMaterial::setSpecularRepeatY(self,shiny_scale_v);
	}
}

//static
void LLPanelFace::syncMaterialRot(LLPanelFace* self, F32 rot, int te)
{
	LLSelectedTEMaterial::setNormalRotation(self,rot * DEG_TO_RAD, te);
	LLSelectedTEMaterial::setSpecularRotation(self,rot * DEG_TO_RAD, te);
	self->sendTextureInfo();
}

//static
void LLPanelFace::onCommitMaterialBumpyRot(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		self->getChild<LLSpinCtrl>("TexRot")->forceSetValue(self->getCurrentBumpyRot());
		syncMaterialRot(self, self->getCurrentBumpyRot());
	}
	else
	{
        if ((bool)self->childGetValue("checkbox planar align").asBoolean())
        {
            LLFace* last_face = NULL;
            bool identical_face = false;
            LLSelectedTE::getFace(last_face, identical_face);
            LLPanelFaceSetAlignedTEFunctor setfunc(self, last_face);
            LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
        }
        else
        {
            LLSelectedTEMaterial::setNormalRotation(self, self->getCurrentBumpyRot() * DEG_TO_RAD);
        }
	}
}

//static
void LLPanelFace::onCommitMaterialShinyRot(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		self->getChild<LLSpinCtrl>("TexRot")->forceSetValue(self->getCurrentShinyRot());
		syncMaterialRot(self, self->getCurrentShinyRot());
	}
	else
	{
        if ((bool)self->childGetValue("checkbox planar align").asBoolean())
        {
            LLFace* last_face = NULL;
            bool identical_face = false;
            LLSelectedTE::getFace(last_face, identical_face);
            LLPanelFaceSetAlignedTEFunctor setfunc(self, last_face);
            LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
        }
        else
        {
            LLSelectedTEMaterial::setSpecularRotation(self, self->getCurrentShinyRot() * DEG_TO_RAD);
        }
	}
}

//static
void LLPanelFace::onCommitMaterialGloss(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setSpecularLightExponent(self,self->getCurrentGlossiness());
}

//static
void LLPanelFace::onCommitMaterialEnv(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	llassert_always(self);
	LLSelectedTEMaterial::setEnvironmentIntensity(self,self->getCurrentEnvIntensity());
}

//static
void LLPanelFace::onCommitMaterialMaskCutoff(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	LLSelectedTEMaterial::setAlphaMaskCutoff(self,self->getCurrentAlphaMaskCutoff());
}

// static
void LLPanelFace::onCommitTextureInfo( LLUICtrl* ctrl, void* userdata )
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	self->sendTextureInfo();
	// vertical scale and repeats per meter depends on each other, so force set on changes
	self->updateUI(true);
}

// static
void LLPanelFace::onCommitTextureScaleX( LLUICtrl* ctrl, void* userdata )
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		F32 bumpy_scale_u = self->getChild<LLUICtrl>("TexScaleU")->getValue().asReal();
		if (self->isIdenticalPlanarTexgen())
		{
			bumpy_scale_u *= 0.5f;
		}
		syncRepeatX(self, bumpy_scale_u);
	}
	else
	{
		self->sendTextureInfo();
	}
	self->updateUI(true);
}

// static
void LLPanelFace::onCommitTextureScaleY( LLUICtrl* ctrl, void* userdata )
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		F32 bumpy_scale_v = self->getChild<LLUICtrl>("TexScaleV")->getValue().asReal();
		if (self->isIdenticalPlanarTexgen())
		{
			bumpy_scale_v *= 0.5f;
		}
		syncRepeatY(self, bumpy_scale_v);
	}
	else
	{
		self->sendTextureInfo();
	}
	self->updateUI(true);
}

// static
void LLPanelFace::onCommitTextureRot( LLUICtrl* ctrl, void* userdata )
{
	LLPanelFace* self = (LLPanelFace*) userdata;

	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncMaterialRot(self, self->getChild<LLUICtrl>("TexRot")->getValue().asReal());
	}
	else
	{
		self->sendTextureInfo();
	}
	self->updateUI(true);
}

// static
void LLPanelFace::onCommitTextureOffsetX( LLUICtrl* ctrl, void* userdata )
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncOffsetX(self, self->getChild<LLUICtrl>("TexOffsetU")->getValue().asReal());
	}
	else
	{
		self->sendTextureInfo();
	}
	self->updateUI(true);
}

// static
void LLPanelFace::onCommitTextureOffsetY( LLUICtrl* ctrl, void* userdata )
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		syncOffsetY(self, self->getChild<LLUICtrl>("TexOffsetV")->getValue().asReal());
	}
	else
	{
		self->sendTextureInfo();
	}
	self->updateUI(true);
}

// Commit the number of repeats per meter
// static
void LLPanelFace::onCommitRepeatsPerMeter(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	
	LLUICtrl*	repeats_ctrl	= self->getChild<LLUICtrl>("rptctrl");
	LLComboBox* combo_matmedia	= self->getChild<LLComboBox>("combobox matmedia");
	LLRadioGroup* radio_mat_type = self->getChild<LLRadioGroup>("radio_material_type");
	
	U32 materials_media = combo_matmedia->getCurrentIndex();

	U32 material_type           = (materials_media == MATMEDIA_MATERIAL) ? radio_mat_type->getSelectedIndex() : 0;
	F32 repeats_per_meter	= repeats_ctrl->getValue().asReal();
	
	F32 obj_scale_s = 1.0f;
	F32 obj_scale_t = 1.0f;

	bool identical_scale_s = false;
	bool identical_scale_t = false;

	LLSelectedTE::getObjectScaleS(obj_scale_s, identical_scale_s);
	LLSelectedTE::getObjectScaleS(obj_scale_t, identical_scale_t);

	LLUICtrl* bumpy_scale_u = self->getChild<LLUICtrl>("bumpyScaleU");
	LLUICtrl* bumpy_scale_v = self->getChild<LLUICtrl>("bumpyScaleV");
	LLUICtrl* shiny_scale_u = self->getChild<LLUICtrl>("shinyScaleU");
	LLUICtrl* shiny_scale_v = self->getChild<LLUICtrl>("shinyScaleV");
 
	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		LLSelectMgr::getInstance()->selectionTexScaleAutofit( repeats_per_meter );

		bumpy_scale_u->setValue(obj_scale_s * repeats_per_meter);
		bumpy_scale_v->setValue(obj_scale_t * repeats_per_meter);

		LLSelectedTEMaterial::setNormalRepeatX(self,obj_scale_s * repeats_per_meter);
		LLSelectedTEMaterial::setNormalRepeatY(self,obj_scale_t * repeats_per_meter);

		shiny_scale_u->setValue(obj_scale_s * repeats_per_meter);
		shiny_scale_v->setValue(obj_scale_t * repeats_per_meter);

		LLSelectedTEMaterial::setSpecularRepeatX(self,obj_scale_s * repeats_per_meter);
		LLSelectedTEMaterial::setSpecularRepeatY(self,obj_scale_t * repeats_per_meter);
	}
	else
	{
		switch (material_type)
		{
			case MATTYPE_DIFFUSE:
			{
				LLSelectMgr::getInstance()->selectionTexScaleAutofit( repeats_per_meter );
			}
			break;

			case MATTYPE_NORMAL:
			{
				bumpy_scale_u->setValue(obj_scale_s * repeats_per_meter);
				bumpy_scale_v->setValue(obj_scale_t * repeats_per_meter);

				LLSelectedTEMaterial::setNormalRepeatX(self,obj_scale_s * repeats_per_meter);
				LLSelectedTEMaterial::setNormalRepeatY(self,obj_scale_t * repeats_per_meter);
			}
			break;

			case MATTYPE_SPECULAR:
			{
				shiny_scale_u->setValue(obj_scale_s * repeats_per_meter);
				shiny_scale_v->setValue(obj_scale_t * repeats_per_meter);

				LLSelectedTEMaterial::setSpecularRepeatX(self,obj_scale_s * repeats_per_meter);
				LLSelectedTEMaterial::setSpecularRepeatY(self,obj_scale_t * repeats_per_meter);
			}
			break;

			default:
				llassert(false);
				break;
		}
	}
	// vertical scale and repeats per meter depends on each other, so force set on changes
	self->updateUI(true);
}

struct LLPanelFaceSetMediaFunctor : public LLSelectedTEFunctor
{
	virtual bool apply(LLViewerObject* object, S32 te)
	{
		viewer_media_t pMediaImpl;
				
		const LLTextureEntry &tep = object->getTEref(te);
		const LLMediaEntry* mep = tep.hasMedia() ? tep.getMediaData() : NULL;
		if ( mep )
		{
			pMediaImpl = LLViewerMedia::getInstance()->getMediaImplFromTextureID(mep->getMediaID());
		}
		
		if ( pMediaImpl.isNull())
		{
			// If we didn't find face media for this face, check whether this face is showing parcel media.
			pMediaImpl = LLViewerMedia::getInstance()->getMediaImplFromTextureID(tep.getID());
		}
		
		if ( pMediaImpl.notNull())
		{
			LLPluginClassMedia *media = pMediaImpl->getMediaPlugin();
			if(media)
			{
				S32 media_width = media->getWidth();
				S32 media_height = media->getHeight();
				S32 texture_width = media->getTextureWidth();
				S32 texture_height = media->getTextureHeight();
				F32 scale_s = (F32)media_width / (F32)texture_width;
				F32 scale_t = (F32)media_height / (F32)texture_height;

				// set scale and adjust offset
				object->setTEScaleS( te, scale_s );
				object->setTEScaleT( te, scale_t );	// don't need to flip Y anymore since QT does this for us now.
				object->setTEOffsetS( te, -( 1.0f - scale_s ) / 2.0f );
				object->setTEOffsetT( te, -( 1.0f - scale_t ) / 2.0f );
			}
		}
		return true;
	};
};

void LLPanelFace::onClickAutoFix(void* userdata)
{
	LLPanelFaceSetMediaFunctor setfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);

	LLPanelFaceSendFunctor sendfunc;
	LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}

void LLPanelFace::onAlignTexture(void* userdata)
{
    LLPanelFace* self = (LLPanelFace*)userdata;
    self->alignTestureLayer();
}

enum EPasteMode
{
    PASTE_COLOR,
    PASTE_TEXTURE
};

struct LLPanelFacePasteTexFunctor : public LLSelectedTEFunctor
{
    LLPanelFacePasteTexFunctor(LLPanelFace* panel, EPasteMode mode) :
        mPanelFace(panel), mMode(mode) {}

    virtual bool apply(LLViewerObject* objectp, S32 te)
    {
        switch (mMode)
        {
        case PASTE_COLOR:
            mPanelFace->onPasteColor(objectp, te);
            break;
        case PASTE_TEXTURE:
            mPanelFace->onPasteTexture(objectp, te);
            break;
        }
        return true;
    }
private:
    LLPanelFace *mPanelFace;
    EPasteMode mMode;
};

struct LLPanelFaceUpdateFunctor : public LLSelectedObjectFunctor
{
    LLPanelFaceUpdateFunctor(bool update_media) : mUpdateMedia(update_media) {}
    virtual bool apply(LLViewerObject* object)
    {
        object->sendTEUpdate();
        if (mUpdateMedia)
        {
            LLVOVolume *vo = dynamic_cast<LLVOVolume*>(object);
            if (vo && vo->hasMedia())
            {
                vo->sendMediaDataUpdate();
            }
        }
        return true;
    }
private:
    bool mUpdateMedia;
};

struct LLPanelFaceNavigateHomeFunctor : public LLSelectedTEFunctor
{
    virtual bool apply(LLViewerObject* objectp, S32 te)
    {
        if (objectp && objectp->getTE(te))
        {
            LLTextureEntry* tep = objectp->getTE(te);
            const LLMediaEntry *media_data = tep->getMediaData();
            if (media_data)
            {
                if (media_data->getCurrentURL().empty() && media_data->getAutoPlay())
                {
                    viewer_media_t media_impl =
                        LLViewerMedia::getInstance()->getMediaImplFromTextureID(tep->getMediaData()->getMediaID());
                    if (media_impl)
                    {
                        media_impl->navigateHome();
                    }
                }
            }
        }
        return true;
    }
};

void LLPanelFace::onCopyColor()
{
    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1)
    {
        return;
    }

    if (mClipboardParams.has("color"))
    {
        mClipboardParams["color"].clear();
    }
    else
    {
        mClipboardParams["color"] = LLSD::emptyArray();
    }

    std::map<LLUUID, LLUUID> asset_item_map;

    // a way to resolve situations where source and target have different amount of faces
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    mClipboardParams["color_all_tes"] = (num_tes != 1) || (LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool());
    for (S32 te = 0; te < num_tes; ++te)
    {
        if (node->isTESelected(te))
        {
            LLTextureEntry* tep = objectp->getTE(te);
            if (tep)
            {
                LLSD te_data;

                // asLLSD() includes media
                te_data["te"] = tep->asLLSD(); // Note: includes a lot more than just color/alpha/glow

                mClipboardParams["color"].append(te_data);
            }
        }
    }
}

void LLPanelFace::onPasteColor()
{
    if (!mClipboardParams.has("color"))
    {
        return;
    }

    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1)
    {
        // not supposed to happen
        LL_WARNS() << "Failed to paste color due to missing or wrong selection" << LL_ENDL;
        return;
    }

    bool face_selection_mode = LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool();
    LLSD &clipboard = mClipboardParams["color"]; // array
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    S32 compare_tes = num_tes;

    if (face_selection_mode)
    {
        compare_tes = 0;
        for (S32 te = 0; te < num_tes; ++te)
        {
            if (node->isTESelected(te))
            {
                compare_tes++;
            }
        }
    }

    // we can copy if single face was copied in edit face mode or if face count matches
    if (!((clipboard.size() == 1) && mClipboardParams["color_all_tes"].asBoolean())
        && compare_tes != clipboard.size())
    {
        LLSD notif_args;
        if (face_selection_mode)
        {
            static std::string reason = getString("paste_error_face_selection_mismatch");
            notif_args["REASON"] = reason;
        }
        else
        {
            static std::string reason = getString("paste_error_object_face_count_mismatch");
            notif_args["REASON"] = reason;
        }
        LLNotificationsUtil::add("FacePasteFailed", notif_args);
        return;
    }

    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();

    LLPanelFacePasteTexFunctor paste_func(this, PASTE_COLOR);
    selected_objects->applyToTEs(&paste_func);

    LLPanelFaceUpdateFunctor sendfunc(false);
    selected_objects->applyToObjects(&sendfunc);
}

void LLPanelFace::onPasteColor(LLViewerObject* objectp, S32 te)
{
    LLSD te_data;
    LLSD &clipboard = mClipboardParams["color"]; // array
    if ((clipboard.size() == 1) && mClipboardParams["color_all_tes"].asBoolean())
    {
        te_data = *(clipboard.beginArray());
    }
    else if (clipboard[te])
    {
        te_data = clipboard[te];
    }
    else
    {
        return;
    }

    LLTextureEntry* tep = objectp->getTE(te);
    if (tep)
    {
        if (te_data.has("te"))
        {
            // Color / Alpha
            if (te_data["te"].has("colors"))
            {
                LLColor4 color = tep->getColor();

                LLColor4 clip_color;
                clip_color.setValue(te_data["te"]["colors"]);

                // Color
                color.mV[VRED] = clip_color.mV[VRED];
                color.mV[VGREEN] = clip_color.mV[VGREEN];
                color.mV[VBLUE] = clip_color.mV[VBLUE];

                // Alpha
                color.mV[VALPHA] = clip_color.mV[VALPHA];

                objectp->setTEColor(te, color);
            }

            // Color/fullbright
            if (te_data["te"].has("fullbright"))
            {
                objectp->setTEFullbright(te, te_data["te"]["fullbright"].asInteger());
            }

            // Glow
            if (te_data["te"].has("glow"))
            {
                objectp->setTEGlow(te, (F32)te_data["te"]["glow"].asReal());
            }
        }
    }
}

void LLPanelFace::onCopyTexture()
{
    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1)
    {
        return;
    }

    if (mClipboardParams.has("texture"))
    {
        mClipboardParams["texture"].clear();
    }
    else
    {
        mClipboardParams["texture"] = LLSD::emptyArray();
    }

    std::map<LLUUID, LLUUID> asset_item_map;

    // a way to resolve situations where source and target have different amount of faces
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    mClipboardParams["texture_all_tes"] = (num_tes != 1) || (LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool());
    for (S32 te = 0; te < num_tes; ++te)
    {
        if (node->isTESelected(te))
        {
            LLTextureEntry* tep = objectp->getTE(te);
            if (tep)
            {
                LLSD te_data;

                // asLLSD() includes media
                te_data["te"] = tep->asLLSD();
                te_data["te"]["shiny"] = tep->getShiny();
                te_data["te"]["bumpmap"] = tep->getBumpmap();
                te_data["te"]["bumpshiny"] = tep->getBumpShiny();
                te_data["te"]["bumpfullbright"] = tep->getBumpShinyFullbright();

                if (te_data["te"].has("imageid"))
                {
                    LLUUID item_id;
                    LLUUID id = te_data["te"]["imageid"].asUUID();
                    bool from_library = get_is_predefined_texture(id);
                    bool full_perm = from_library;

                    if (!full_perm
                        && objectp->permCopy()
                        && objectp->permTransfer()
                        && objectp->permModify())
                    {
                        // If agent created this object and nothing is limiting permissions, mark as full perm
                        // If agent was granted permission to edit objects owned and created by somebody else, mark full perm
                        // This check is not perfect since we can't figure out whom textures belong to so this ended up restrictive
                        std::string creator_app_link;
                        LLUUID creator_id;
                        LLSelectMgr::getInstance()->selectGetCreator(creator_id, creator_app_link);
                        full_perm = objectp->mOwnerID == creator_id;
                    }

                    if (id.notNull() && !full_perm)
                    {
                        std::map<LLUUID, LLUUID>::iterator iter = asset_item_map.find(id);
                        if (iter != asset_item_map.end())
                        {
                            item_id = iter->second;
                        }
                        else
                        {
                            // What this does is simply searches inventory for item with same asset id,
                            // as result it is Hightly unreliable, leaves little control to user, borderline hack
                            // but there are little options to preserve permissions - multiple inventory
                            // items might reference same asset and inventory search is expensive.
                            bool no_transfer = false;
                            if (objectp->getInventoryItemByAsset(id))
                            {
                                no_transfer = !objectp->getInventoryItemByAsset(id)->getIsFullPerm();
                            }
                            item_id = get_copy_free_item_by_asset_id(id, no_transfer);
                            // record value to avoid repeating inventory search when possible
                            asset_item_map[id] = item_id;
                        }
                    }

                    if (item_id.notNull() && gInventory.isObjectDescendentOf(item_id, gInventory.getLibraryRootFolderID()))
                    {
                        full_perm = true;
                        from_library = true;
                    }

                    {
                        te_data["te"]["itemfullperm"] = full_perm;
                        te_data["te"]["fromlibrary"] = from_library; 

                        // If full permission object, texture is free to copy,
                        // but otherwise we need to check inventory and extract permissions
                        //
                        // Normally we care only about restrictions for current user and objects
                        // don't inherit any 'next owner' permissions from texture, so there is
                        // no need to record item id if full_perm==true
                        if (!full_perm && !from_library && item_id.notNull())
                        {
                            LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
                            if (itemp)
                            {
                                LLPermissions item_permissions = itemp->getPermissions();
                                if (item_permissions.allowOperationBy(PERM_COPY,
                                    gAgent.getID(),
                                    gAgent.getGroupID()))
                                {
                                    te_data["te"]["imageitemid"] = item_id;
                                    te_data["te"]["itemfullperm"] = itemp->getIsFullPerm();
                                    if (!itemp->isFinished())
                                    {
                                        // needed for dropTextureAllFaces
                                        LLInventoryModelBackgroundFetch::instance().start(item_id, false);
                                    }
                                }
                            }
                        }
                    }
                }

                LLMaterialPtr material_ptr = tep->getMaterialParams();
                if (!material_ptr.isNull())
                {
                    LLSD mat_data;

                    mat_data["NormMap"] = material_ptr->getNormalID();
                    mat_data["SpecMap"] = material_ptr->getSpecularID();

                    mat_data["NormRepX"] = material_ptr->getNormalRepeatX();
                    mat_data["NormRepY"] = material_ptr->getNormalRepeatY();
                    mat_data["NormOffX"] = material_ptr->getNormalOffsetX();
                    mat_data["NormOffY"] = material_ptr->getNormalOffsetY();
                    mat_data["NormRot"] = material_ptr->getNormalRotation();

                    mat_data["SpecRepX"] = material_ptr->getSpecularRepeatX();
                    mat_data["SpecRepY"] = material_ptr->getSpecularRepeatY();
                    mat_data["SpecOffX"] = material_ptr->getSpecularOffsetX();
                    mat_data["SpecOffY"] = material_ptr->getSpecularOffsetY();
                    mat_data["SpecRot"] = material_ptr->getSpecularRotation();

                    mat_data["SpecColor"] = material_ptr->getSpecularLightColor().getValue();
                    mat_data["SpecExp"] = material_ptr->getSpecularLightExponent();
                    mat_data["EnvIntensity"] = material_ptr->getEnvironmentIntensity();
                    mat_data["AlphaMaskCutoff"] = material_ptr->getAlphaMaskCutoff();
                    mat_data["DiffuseAlphaMode"] = material_ptr->getDiffuseAlphaMode();

                    // Replace no-copy textures, destination texture will get used instead if available
                    if (mat_data.has("NormMap"))
                    {
                        LLUUID id = mat_data["NormMap"].asUUID();
                        if (id.notNull() && !get_can_copy_texture(id))
                        {
                            mat_data["NormMap"] = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
                            mat_data["NormMapNoCopy"] = true;
                        }

                    }
                    if (mat_data.has("SpecMap"))
                    {
                        LLUUID id = mat_data["SpecMap"].asUUID();
                        if (id.notNull() && !get_can_copy_texture(id))
                        {
                            mat_data["SpecMap"] = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
                            mat_data["SpecMapNoCopy"] = true;
                        }

                    }

                    te_data["material"] = mat_data;
                }

                mClipboardParams["texture"].append(te_data);
            }
        }
    }
}

void LLPanelFace::onPasteTexture()
{
    if (!mClipboardParams.has("texture"))
    {
        return;
    }

    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1)
    {
        // not supposed to happen
        LL_WARNS() << "Failed to paste texture due to missing or wrong selection" << LL_ENDL;
        return;
    }

    bool face_selection_mode = LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool();
    LLSD &clipboard = mClipboardParams["texture"]; // array
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    S32 compare_tes = num_tes;

    if (face_selection_mode)
    {
        compare_tes = 0;
        for (S32 te = 0; te < num_tes; ++te)
        {
            if (node->isTESelected(te))
            {
                compare_tes++;
            }
        }
    }

    // we can copy if single face was copied in edit face mode or if face count matches
    if (!((clipboard.size() == 1) && mClipboardParams["texture_all_tes"].asBoolean()) 
        && compare_tes != clipboard.size())
    {
        LLSD notif_args;
        if (face_selection_mode)
        {
            static std::string reason = getString("paste_error_face_selection_mismatch");
            notif_args["REASON"] = reason;
        }
        else
        {
            static std::string reason = getString("paste_error_object_face_count_mismatch");
            notif_args["REASON"] = reason;
        }
        LLNotificationsUtil::add("FacePasteFailed", notif_args);
        return;
    }

    bool full_perm_object = true;
    LLSD::array_const_iterator iter = clipboard.beginArray();
    LLSD::array_const_iterator end = clipboard.endArray();
    for (; iter != end; ++iter)
    {
        const LLSD& te_data = *iter;
        if (te_data.has("te") && te_data["te"].has("imageid"))
        {
            bool full_perm = te_data["te"].has("itemfullperm") && te_data["te"]["itemfullperm"].asBoolean();
            full_perm_object &= full_perm;
            if (!full_perm)
            {
                if (te_data["te"].has("imageitemid"))
                {
                    LLUUID item_id = te_data["te"]["imageitemid"].asUUID();
                    if (item_id.notNull())
                    {
                        LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
                        if (!itemp)
                        {
                            // image might be in object's inventory, but it can be not up to date
                            LLSD notif_args;
                            static std::string reason = getString("paste_error_inventory_not_found");
                            notif_args["REASON"] = reason;
                            LLNotificationsUtil::add("FacePasteFailed", notif_args);
                            return;
                        }
                    }
                }
                else
                {
                    // Item was not found on 'copy' stage
                    // Since this happened at copy, might be better to either show this
                    // at copy stage or to drop clipboard here
                    LLSD notif_args;
                    static std::string reason = getString("paste_error_inventory_not_found");
                    notif_args["REASON"] = reason;
                    LLNotificationsUtil::add("FacePasteFailed", notif_args);
                    return;
                }
            }
        }
    }

    if (!full_perm_object)
    {
        LLNotificationsUtil::add("FacePasteTexturePermissions");
    }

    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();

    LLPanelFacePasteTexFunctor paste_func(this, PASTE_TEXTURE);
    selected_objects->applyToTEs(&paste_func);

    LLPanelFaceUpdateFunctor sendfunc(true);
    selected_objects->applyToObjects(&sendfunc);

    LLPanelFaceNavigateHomeFunctor navigate_home_func;
    selected_objects->applyToTEs(&navigate_home_func);
}

void LLPanelFace::onPasteTexture(LLViewerObject* objectp, S32 te)
{
    LLSD te_data;
    LLSD &clipboard = mClipboardParams["texture"]; // array
    if ((clipboard.size() == 1) && mClipboardParams["texture_all_tes"].asBoolean())
    {
        te_data = *(clipboard.beginArray());
    }
    else if (clipboard[te])
    {
        te_data = clipboard[te];
    }
    else
    {
        return;
    }

    LLTextureEntry* tep = objectp->getTE(te);
    if (tep)
    {
        if (te_data.has("te"))
        {
            // Texture
            bool full_perm = te_data["te"].has("itemfullperm") && te_data["te"]["itemfullperm"].asBoolean();
            bool from_library = te_data["te"].has("fromlibrary") && te_data["te"]["fromlibrary"].asBoolean();
            if (te_data["te"].has("imageid"))
            {
                const LLUUID& imageid = te_data["te"]["imageid"].asUUID(); //texture or asset id
                LLViewerInventoryItem* itemp_res = NULL;

                if (te_data["te"].has("imageitemid"))
                {
                    LLUUID item_id = te_data["te"]["imageitemid"].asUUID();
                    if (item_id.notNull())
                    {
                        LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
                        if (itemp && itemp->isFinished())
                        {
                            // dropTextureAllFaces will fail if incomplete
                            itemp_res = itemp;
                        }
                        else
                        {
                            // Theoretically shouldn't happend, but if it does happen, we
                            // might need to add a notification to user that paste will fail
                            // since inventory isn't fully loaded
                            LL_WARNS() << "Item " << item_id << " is incomplete, paste might fail silently." << LL_ENDL;
                        }
                    }
                }
                // for case when item got removed from inventory after we pressed 'copy'
                // or texture got pasted into previous object
                if (!itemp_res && !full_perm)
                {
                    // Due to checks for imageitemid in LLPanelFace::onPasteTexture() this should no longer be reachable.
                    LL_INFOS() << "Item " << te_data["te"]["imageitemid"].asUUID() << " no longer in inventory." << LL_ENDL;
                    // Todo: fix this, we are often searching same texture multiple times (equal to number of faces)
                    // Perhaps just mPanelFace->onPasteTexture(objectp, te, &asset_to_item_id_map); ? Not pretty, but will work
                    LLViewerInventoryCategory::cat_array_t cats;
                    LLViewerInventoryItem::item_array_t items;
                    LLAssetIDMatches asset_id_matches(imageid);
                    gInventory.collectDescendentsIf(LLUUID::null,
                        cats,
                        items,
                        LLInventoryModel::INCLUDE_TRASH,
                        asset_id_matches);

                    // Extremely unreliable and perfomance unfriendly.
                    // But we need this to check permissions and it is how texture control finds items
                    for (S32 i = 0; i < items.size(); i++)
                    {
                        LLViewerInventoryItem* itemp = items[i];
                        if (itemp && itemp->isFinished())
                        {
                            // dropTextureAllFaces will fail if incomplete
                            LLPermissions item_permissions = itemp->getPermissions();
                            if (item_permissions.allowOperationBy(PERM_COPY,
                                gAgent.getID(),
                                gAgent.getGroupID()))
                            {
                                itemp_res = itemp;
                                break; // first match
                            }
                        }
                    }
                }

                if (itemp_res)
                {
                    if (te == -1) // all faces
                    {
                        LLToolDragAndDrop::dropTextureAllFaces(objectp,
                            itemp_res,
                            from_library ? LLToolDragAndDrop::SOURCE_LIBRARY : LLToolDragAndDrop::SOURCE_AGENT,
                            LLUUID::null);
                    }
                    else // one face
                    {
                        LLToolDragAndDrop::dropTextureOneFace(objectp,
                            te,
                            itemp_res,
                            from_library ? LLToolDragAndDrop::SOURCE_LIBRARY : LLToolDragAndDrop::SOURCE_AGENT,
                            LLUUID::null,
                            0);
                    }
                }
                // not an inventory item or no complete items
                else if (full_perm)
                {
                    // Either library, local or existed as fullperm when user made a copy
                    LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture(imageid, FTT_DEFAULT, TRUE, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
                    objectp->setTEImage(U8(te), image);
                }
            }

            if (te_data["te"].has("bumpmap"))
            {
                objectp->setTEBumpmap(te, (U8)te_data["te"]["bumpmap"].asInteger());
            }
            if (te_data["te"].has("bumpshiny"))
            {
                objectp->setTEBumpShiny(te, (U8)te_data["te"]["bumpshiny"].asInteger());
            }
            if (te_data["te"].has("bumpfullbright"))
            {
                objectp->setTEBumpShinyFullbright(te, (U8)te_data["te"]["bumpfullbright"].asInteger());
            }

            // Texture map
            if (te_data["te"].has("scales") && te_data["te"].has("scalet"))
            {
                objectp->setTEScale(te, (F32)te_data["te"]["scales"].asReal(), (F32)te_data["te"]["scalet"].asReal());
            }
            if (te_data["te"].has("offsets") && te_data["te"].has("offsett"))
            {
                objectp->setTEOffset(te, (F32)te_data["te"]["offsets"].asReal(), (F32)te_data["te"]["offsett"].asReal());
            }
            if (te_data["te"].has("imagerot"))
            {
                objectp->setTERotation(te, (F32)te_data["te"]["imagerot"].asReal());
            }

            // Media
            if (te_data["te"].has("media_flags"))
            {
                U8 media_flags = te_data["te"]["media_flags"].asInteger();
                objectp->setTEMediaFlags(te, media_flags);
                LLVOVolume *vo = dynamic_cast<LLVOVolume*>(objectp);
                if (vo && te_data["te"].has(LLTextureEntry::TEXTURE_MEDIA_DATA_KEY))
                {
                    vo->syncMediaData(te, te_data["te"][LLTextureEntry::TEXTURE_MEDIA_DATA_KEY], true/*merge*/, true/*ignore_agent*/);
                }
            }
            else
            {
                // Keep media flags on destination unchanged
            }
        }

        if (te_data.has("material"))
        {
            LLUUID object_id = objectp->getID();

            LLSelectedTEMaterial::setAlphaMaskCutoff(this, (U8)te_data["material"]["SpecRot"].asInteger(), te, object_id);

            // Normal
            // Replace placeholders with target's
            if (te_data["material"].has("NormMapNoCopy"))
            {
                LLMaterialPtr material = tep->getMaterialParams();
                if (material.notNull())
                {
                    LLUUID id = material->getNormalID();
                    if (id.notNull())
                    {
                        te_data["material"]["NormMap"] = id;
                    }
                }
            }
            LLSelectedTEMaterial::setNormalID(this, te_data["material"]["NormMap"].asUUID(), te, object_id);
            LLSelectedTEMaterial::setNormalRepeatX(this, (F32)te_data["material"]["NormRepX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalRepeatY(this, (F32)te_data["material"]["NormRepY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalOffsetX(this, (F32)te_data["material"]["NormOffX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalOffsetY(this, (F32)te_data["material"]["NormOffY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalRotation(this, (F32)te_data["material"]["NormRot"].asReal(), te, object_id);

            // Specular
                // Replace placeholders with target's
            if (te_data["material"].has("SpecMapNoCopy"))
            {
                LLMaterialPtr material = tep->getMaterialParams();
                if (material.notNull())
                {
                    LLUUID id = material->getSpecularID();
                    if (id.notNull())
                    {
                        te_data["material"]["SpecMap"] = id;
                    }
                }
            }
            LLSelectedTEMaterial::setSpecularID(this, te_data["material"]["SpecMap"].asUUID(), te, object_id);
            LLSelectedTEMaterial::setSpecularRepeatX(this, (F32)te_data["material"]["SpecRepX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularRepeatY(this, (F32)te_data["material"]["SpecRepY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularOffsetX(this, (F32)te_data["material"]["SpecOffX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularOffsetY(this, (F32)te_data["material"]["SpecOffY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularRotation(this, (F32)te_data["material"]["SpecRot"].asReal(), te, object_id);
            LLColor4 spec_color(te_data["material"]["SpecColor"]);
            LLSelectedTEMaterial::setSpecularLightColor(this, spec_color, te);
            LLSelectedTEMaterial::setSpecularLightExponent(this, (U8)te_data["material"]["SpecExp"].asInteger(), te, object_id);
            LLSelectedTEMaterial::setEnvironmentIntensity(this, (U8)te_data["material"]["EnvIntensity"].asInteger(), te, object_id);
            LLSelectedTEMaterial::setDiffuseAlphaMode(this, (U8)te_data["material"]["SpecRot"].asInteger(), te, object_id);
            if (te_data.has("te") && te_data["te"].has("shiny"))
            {
                objectp->setTEShiny(te, (U8)te_data["te"]["shiny"].asInteger());
            }
        }
    }
}

// <FS> Extended copy & paste buttons
//void LLPanelFace::menuDoToSelected(const LLSD& userdata)
//{
//    std::string command = userdata.asString();
//
//    // paste
//    if (command == "color_paste")
//    {
//        onPasteColor();
//    }
//    else if (command == "texture_paste")
//    {
//        onPasteTexture();
//    }
//    // copy
//    else if (command == "color_copy")
//    {
//        onCopyColor();
//    }
//    else if (command == "texture_copy")
//    {
//        onCopyTexture();
//    }
//}
//
//bool LLPanelFace::menuEnableItem(const LLSD& userdata)
//{
//    std::string command = userdata.asString();
//
//    // paste options
//    if (command == "color_paste")
//    {
//        return mClipboardParams.has("color");
//    }
//    else if (command == "texture_paste")
//    {
//        return mClipboardParams.has("texture");
//    }
//    return false;
//}
void LLPanelFace::onCopyFaces()
{
	onCopyTexture();
	onCopyColor();
	mBtnPasteFaces->setEnabled(!mClipboardParams.emptyMap() && (mClipboardParams.has("color") || mClipboardParams.has("texture")));
}

void LLPanelFace::onPasteFaces()
{
	LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();

	LLPanelFacePasteTexFunctor paste_texture_func(this, PASTE_TEXTURE);
	selected_objects->applyToTEs(&paste_texture_func);

	LLPanelFacePasteTexFunctor paste_color_func(this, PASTE_COLOR);
	selected_objects->applyToTEs(&paste_color_func);

	LLPanelFaceUpdateFunctor sendfunc(true);
	selected_objects->applyToObjects(&sendfunc);

	LLPanelFaceNavigateHomeFunctor navigate_home_func;
	selected_objects->applyToTEs(&navigate_home_func);
}
// </FS>


// TODO: I don't know who put these in or what these are for???
void LLPanelFace::setMediaURL(const std::string& url)
{
}
void LLPanelFace::setMediaType(const std::string& mime_type)
{
}

// static
void LLPanelFace::onCommitPlanarAlign(LLUICtrl* ctrl, void* userdata)
{
	LLPanelFace* self = (LLPanelFace*) userdata;
	self->getState();
	self->sendTextureInfo();
}

void LLPanelFace::onTextureSelectionChanged(LLInventoryItem* itemp)
{
	LL_DEBUGS("Materials") << "item asset " << itemp->getAssetUUID() << LL_ENDL;
	//LLRadioGroup* radio_mat_type = findChild<LLRadioGroup>("radio_material_type");
	//if(!radio_mat_type)
	if (!mRadioMatType)
	{
	    return;
	}
	U32 mattype = mRadioMatType->getSelectedIndex();
	std::string which_control="texture control";
	switch (mattype)
	{
		case MATTYPE_SPECULAR:
			which_control = "shinytexture control";
			break;
		case MATTYPE_NORMAL:
			which_control = "bumpytexture control";
			break;
		// no default needed
	}
	LL_DEBUGS("Materials") << "control " << which_control << LL_ENDL;
	LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>(which_control);
	if (texture_ctrl)
	{
		LLUUID obj_owner_id;
		std::string obj_owner_name;
		LLSelectMgr::instance().selectGetOwner(obj_owner_id, obj_owner_name);

		LLSaleInfo sale_info;
		LLSelectMgr::instance().selectGetSaleInfo(sale_info);

		bool can_copy = itemp->getPermissions().allowCopyBy(gAgentID); // do we have perm to copy this texture?
		bool can_transfer = itemp->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID); // do we have perm to transfer this texture?
		bool is_object_owner = gAgentID == obj_owner_id; // does object for which we are going to apply texture belong to the agent?
		bool not_for_sale = !sale_info.isForSale(); // is object for which we are going to apply texture not for sale?

		if (can_copy && can_transfer)
		{
			texture_ctrl->setCanApply(true, true);
			return;
		}

		// if texture has (no-transfer) attribute it can be applied only for object which we own and is not for sale
		texture_ctrl->setCanApply(false, can_transfer ? true : is_object_owner && not_for_sale);

		if (gSavedSettings.getBOOL("TextureLivePreview"))
		{
			LLNotificationsUtil::add("LivePreviewUnavailable");
		}
	}
}

bool LLPanelFace::isIdenticalPlanarTexgen()
{
	LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
	bool identical_texgen = false;
	LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
	return (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
}

void LLPanelFace::LLSelectedTE::getFace(LLFace*& face_to_return, bool& identical_face)
{		
	struct LLSelectedTEGetFace : public LLSelectedTEGetFunctor<LLFace *>
	{
		LLFace* get(LLViewerObject* object, S32 te)
		{
			return (object->mDrawable) ? object->mDrawable->getFace(te): NULL;
		}
	} get_te_face_func;
	identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_te_face_func, face_to_return, false, (LLFace*)nullptr);
}

void LLPanelFace::LLSelectedTE::getImageFormat(LLGLenum& image_format_to_return, bool& identical_face)
{
	LLGLenum image_format;
	struct LLSelectedTEGetImageFormat : public LLSelectedTEGetFunctor<LLGLenum>
	{
		LLGLenum get(LLViewerObject* object, S32 te_index)
		{
			LLViewerTexture* image = object->getTEImage(te_index);
			return image ? image->getPrimaryFormat() : GL_RGB;
		}
	} get_glenum;
	identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_glenum, image_format);
	image_format_to_return = image_format;
}

void LLPanelFace::LLSelectedTE::getTexId(LLUUID& id, bool& identical)
{		
	struct LLSelectedTEGetTexId : public LLSelectedTEGetFunctor<LLUUID>
	{
		LLUUID get(LLViewerObject* object, S32 te_index)
		{
			LLTextureEntry *te = object->getTE(te_index);
			if (te)
			{
				if ((te->getID() == IMG_USE_BAKED_EYES) || (te->getID() == IMG_USE_BAKED_HAIR) || (te->getID() == IMG_USE_BAKED_HEAD) || (te->getID() == IMG_USE_BAKED_LOWER) || (te->getID() == IMG_USE_BAKED_SKIRT) || (te->getID() == IMG_USE_BAKED_UPPER)
					|| (te->getID() == IMG_USE_BAKED_LEFTARM) || (te->getID() == IMG_USE_BAKED_LEFTLEG) || (te->getID() == IMG_USE_BAKED_AUX1) || (te->getID() == IMG_USE_BAKED_AUX2) || (te->getID() == IMG_USE_BAKED_AUX3))
				{
					return te->getID();
				}
			}

			LLUUID id;
			LLViewerTexture* image = object->getTEImage(te_index);
			if (image)
			{
				id = image->getID();
			}

			if (!id.isNull() && LLViewerMedia::getInstance()->textureHasMedia(id))
			{
				if (te)
				{
					LLViewerTexture* tex = te->getID().notNull() ? gTextureList.findImage(te->getID(), TEX_LIST_STANDARD) : NULL;
					if(!tex)
					{
						tex = LLViewerFetchedTexture::sDefaultImagep;
					}
					if (tex)
					{
						id = tex->getID();
					}
				}
			}
			return id;
		}
	} func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, id );
}

void LLPanelFace::LLSelectedTEMaterial::getCurrent(LLMaterialPtr& material_ptr, bool& identical_material)
{
	struct MaterialFunctor : public LLSelectedTEGetFunctor<LLMaterialPtr>
	{
		LLMaterialPtr get(LLViewerObject* object, S32 te_index)
		{
			return object->getTEref(te_index).getMaterialParams();
		}
	} func;
	identical_material = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, material_ptr);
}

void LLPanelFace::LLSelectedTEMaterial::getMaxSpecularRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxSpecRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			LLMaterial* mat = object->getTEref(face).getMaterialParams().get();
			U32 s_axis = VX;
			U32 t_axis = VY;
			F32 repeats_s = 1.0f;
			F32 repeats_t = 1.0f;
			if (mat)
			{
				mat->getSpecularRepeat(repeats_s, repeats_t);
				repeats_s /= object->getScale().mV[s_axis];
				repeats_t /= object->getScale().mV[t_axis];
			}					
			return llmax(repeats_s, repeats_t);
		}

	} max_spec_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_spec_repeats_func, repeats);
}

void LLPanelFace::LLSelectedTEMaterial::getMaxNormalRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxNormRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			LLMaterial* mat = object->getTEref(face).getMaterialParams().get();
			U32 s_axis = VX;
			U32 t_axis = VY;
			F32 repeats_s = 1.0f;
			F32 repeats_t = 1.0f;
			if (mat)
			{
				mat->getNormalRepeat(repeats_s, repeats_t);
				repeats_s /= object->getScale().mV[s_axis];
				repeats_t /= object->getScale().mV[t_axis];
			}					
			return llmax(repeats_s, repeats_t);
		}

	} max_norm_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_norm_repeats_func, repeats);
}

void LLPanelFace::LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(U8& diffuse_alpha_mode, bool& identical, bool diffuse_texture_has_alpha)
{
	struct LLSelectedTEGetDiffuseAlphaMode : public LLSelectedTEGetFunctor<U8>
	{
		LLSelectedTEGetDiffuseAlphaMode() : _isAlpha(false) {}
		LLSelectedTEGetDiffuseAlphaMode(bool diffuse_texture_has_alpha) : _isAlpha(diffuse_texture_has_alpha) {}
		virtual ~LLSelectedTEGetDiffuseAlphaMode() {}

		U8 get(LLViewerObject* object, S32 face)
		{
			U8 diffuse_mode = _isAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

			LLTextureEntry* tep = object->getTE(face);
			if (tep)
			{
				LLMaterial* mat = tep->getMaterialParams().get();
				if (mat)
				{
					diffuse_mode = mat->getDiffuseAlphaMode();
				}
			}
			
			return diffuse_mode;
		}
		bool _isAlpha; // whether or not the diffuse texture selected contains alpha information
	} get_diff_mode(diffuse_texture_has_alpha);
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &get_diff_mode, diffuse_alpha_mode);
}

void LLPanelFace::LLSelectedTE::getObjectScaleS(F32& scale_s, bool& identical)
{	
	struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[s_axis];
		}

	} scale_s_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_s_func, scale_s );
}

void LLPanelFace::LLSelectedTE::getObjectScaleT(F32& scale_t, bool& identical)
{	
	struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			return object->getScale().mV[t_axis];
		}

	} scale_t_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_t_func, scale_t );
}

void LLPanelFace::LLSelectedTE::getMaxDiffuseRepeats(F32& repeats, bool& identical)
{
	struct LLSelectedTEGetMaxDiffuseRepeats : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face)
		{
			U32 s_axis = VX;
			U32 t_axis = VY;
			LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
			F32 repeats_s = object->getTEref(face).mScaleS / object->getScale().mV[s_axis];
			F32 repeats_t = object->getTEref(face).mScaleT / object->getScale().mV[t_axis];
			return llmax(repeats_s, repeats_t);
		}

	} max_diff_repeats_func;
	identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_diff_repeats_func, repeats );
}

// <FS:CR> Materials alignment
//static
void LLPanelFace::onClickMapsSync(LLUICtrl* ctrl, void *userdata)
{
	LLPanelFace *self = (LLPanelFace*) userdata;
	llassert_always(self);
	self->getState();
	if (gSavedSettings.getBOOL("SyncMaterialSettings"))
	{
		alignMaterialsProperties(self);
	}
}

//static
void LLPanelFace::alignMaterialsProperties(LLPanelFace* self)
{
	// <FS:TS> FIRE-11911: Synchronize materials doesn't work with planar textures
	//  Don't even try to do the alignment if we wind up here and planar is enabled.
	if ((bool)self->childGetValue("checkbox planar align").asBoolean())
	{
		return;
	}
	// </FS:TS> FIRE-11911

	//LLPanelFace *self = (LLPanelFace*) userdata;
	llassert_always(self);
	
	F32 tex_scale_u =	self->getCurrentTextureScaleU();
	F32 tex_scale_v =	self->getCurrentTextureScaleV();
	F32 tex_offset_u =	self->getCurrentTextureOffsetU();
	F32 tex_offset_v =	self->getCurrentTextureOffsetV();
	F32 tex_rot =		self->getCurrentTextureRot();

	//<FS:TS> FIRE-12275: Material offset not working correctly
	//  Since the server cannot store negative offsets for materials
	//  textures, we normalize them to equivalent positive values here.
	tex_offset_u = (tex_offset_u < 0.0) ? 1.0+tex_offset_u : tex_offset_u;
	tex_offset_v = (tex_offset_v < 0.0) ? 1.0+tex_offset_v : tex_offset_v;
	//</FS:TS> FIRE-12275

	//<FS:TS> FIRE-12831: Negative rotations revert to zero
	//  The same goes for rotations as for offsets.
	tex_rot = (tex_rot < 0.0) ? 360.0+tex_rot : tex_rot;
	//</FS:TS> FIRE-12831

	self->childSetValue("shinyScaleU",	tex_scale_u);
	self->childSetValue("shinyScaleV",	tex_scale_v);
	self->childSetValue("shinyOffsetU",	tex_offset_u);
	self->childSetValue("shinyOffsetV",	tex_offset_v);
	self->childSetValue("shinyRot",		tex_rot);
		
	LLSelectedTEMaterial::setSpecularRepeatX(self, tex_scale_u);
	LLSelectedTEMaterial::setSpecularRepeatY(self, tex_scale_v);
	LLSelectedTEMaterial::setSpecularOffsetX(self, tex_offset_u);
	LLSelectedTEMaterial::setSpecularOffsetY(self, tex_offset_v);
	LLSelectedTEMaterial::setSpecularRotation(self, tex_rot * DEG_TO_RAD);

	self->childSetValue("bumpyScaleU",	tex_scale_u);
	self->childSetValue("bumpyScaleV",	tex_scale_v);
	self->childSetValue("bumpyOffsetU",	tex_offset_u);
	self->childSetValue("bumpyOffsetV",	tex_offset_v);
	self->childSetValue("bumpyRot",		tex_rot);
		
	LLSelectedTEMaterial::setNormalRepeatX(self, tex_scale_u);
	LLSelectedTEMaterial::setNormalRepeatY(self, tex_scale_v);
	LLSelectedTEMaterial::setNormalOffsetX(self, tex_offset_u);
	LLSelectedTEMaterial::setNormalOffsetY(self, tex_offset_v);
	LLSelectedTEMaterial::setNormalRotation(self, tex_rot * DEG_TO_RAD);
}

// <FS:CR> FIRE-11407 - Flip buttons
//static
void LLPanelFace::onCommitFlip(const LLUICtrl* ctrl, const LLSD& user_data)
{
	LLPanelFace* self = dynamic_cast<LLPanelFace*>(ctrl->getParent());
	llassert_always(self);
	
	LLRadioGroup* radio_mattype = self->findChild<LLRadioGroup>("radio_material_type");
	if (!radio_mattype) return;
	
	std::string user_data_string = user_data.asString();
	if (user_data_string.empty()) return;
	std::string control_name = "";
	U32 mattype = radio_mattype->getSelectedIndex();
	switch (mattype)
	{
		case MATTYPE_DIFFUSE:
			control_name = "TexScale" + user_data_string;
			break;
		case MATTYPE_NORMAL:
			control_name = "bumpyScale" + user_data_string;
			break;
		case MATTYPE_SPECULAR:
			control_name = "shinyScale" + user_data_string;
			break;
		default:
			llassert(mattype);
			return;
	}
	
	LLUICtrl* spinner = self->findChild<LLUICtrl>(control_name);
	if (spinner)
	{
		F32 value = -(spinner->getValue().asReal());
		spinner->setValue(value);
		
		switch (radio_mattype->getSelectedIndex())
		{
			case MATTYPE_DIFFUSE:
				self->sendTextureInfo();
				if (gSavedSettings.getBOOL("SyncMaterialSettings"))
				{
					alignMaterialsProperties(self);
				}
				break;
			case MATTYPE_NORMAL:
				if (user_data_string == "U")
					LLSelectedTEMaterial::setNormalRepeatX(self,value);
				else if (user_data_string == "V")
					LLSelectedTEMaterial::setNormalRepeatY(self,value);
				break;
			case MATTYPE_SPECULAR:
				if (user_data_string == "U")
					LLSelectedTEMaterial::setSpecularRepeatX(self,value);
				else if (user_data_string == "V")
					LLSelectedTEMaterial::setSpecularRepeatY(self,value);
				break;
			default:
				llassert(mattype);
				return;
		}
	}
}

void LLPanelFace::changePrecision(S32 decimal_precision)
{
	mCtrlTexScaleU->setPrecision(decimal_precision);
	mCtrlTexScaleV->setPrecision(decimal_precision);
	mCtrlBumpyScaleU->setPrecision(decimal_precision);
	mCtrlBumpyScaleV->setPrecision(decimal_precision);
	mCtrlShinyScaleU->setPrecision(decimal_precision);
	mCtrlShinyScaleV->setPrecision(decimal_precision);
	mCtrlTexOffsetU->setPrecision(decimal_precision);
	mCtrlTexOffsetV->setPrecision(decimal_precision);
	mCtrlBumpyOffsetU->setPrecision(decimal_precision);
	mCtrlBumpyOffsetV->setPrecision(decimal_precision);
	mCtrlShinyOffsetU->setPrecision(decimal_precision);
	mCtrlShinyOffsetV->setPrecision(decimal_precision);
	mCtrlTexRot->setPrecision(decimal_precision);
	mCtrlBumpyRot->setPrecision(decimal_precision);
	mCtrlShinyRot->setPrecision(decimal_precision);
	mCtrlRpt->setPrecision(decimal_precision);
}
// </FS:CR>
