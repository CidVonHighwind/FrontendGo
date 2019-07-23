#include "OvrApp.h"

#include <dirent.h>
#include <sstream>
#include <fstream>

#include "Global.h"
#include "DrawHelper.h"
#include "FontMaster.h"
#include "LayerBuilder.h"
#include "TextureLoader.h"
#include "Audio/OpenSLWrap.h"
#include "MenuHelper.h"

#include "Emulator.h"
#include "Menu.h"

using namespace OVR;

FontManager::RenderFont fontHeader, fontBattery, fontTime, fontMenu, fontList, fontBottom,
        fontSlot, fontVersion, fontSmall;

GLuint textureBackgroundId, textureIdMenu, textureHeaderIconId, textureGbIconId,
        textureGbcIconId, textureVbIconId,
        textureSaveIconId,
        textureLoadIconId, textureWhiteId, textureResumeId, textureSettingsId, texuterLeftRightIconId,
        textureUpDownIconId, textureResetIconId, textureSaveSlotIconId, textureLoadRomIconId,
        textureBackIconId, textureMoveIconId, textureDistanceIconId, textureResetViewIconId,
        textureScaleIconId, textureMappingIconId, texturePaletteIconId, textureButtonAIconId,
        textureButtonBIconId, textureButtonXIconId, textureButtonYIconId, textureFollowHeadIconId,
        textureDMGIconId, textureExitIconId, threedeeIconId, twodeeIconId, mappingLeftDownId, textureIpdIconId,
        mappingLeftUpId, mappingLeftLeftId, mappingLeftRightId, mappingRightDownId, mappingRightUpId,
        mappingRightLeftId, mappingRightRightId, mappingStartId, mappingSelectId, mappingTriggerLeft, mappingTriggerRight;

std::string appStoragePath, saveFilePath, romFolderPath;

template<typename T>
std::string to_string(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}


OvrApp::OvrApp()
        : SoundEffectPlayer(NULL),
          GuiSys(OvrGuiSys::Create()),
          Locale(NULL) {
}

OvrApp::~OvrApp() {
    delete SoundEffectPlayer;
    SoundEffectPlayer = NULL;

    OvrGuiSys::Destroy(GuiSys);

    OVR_LOG("Closed OvrApp");
}

void OvrApp::Configure(ovrSettings &settings) {
    settings.CpuLevel = 0;
    settings.GpuLevel = 0;

    // Default to 2x MSAA.
    settings.EyeBufferParms.colorFormat = COLOR_8888_sRGB;
    settings.EyeBufferParms.multisamples = 4;

    settings.RenderMode = RENDERMODE_MULTIVIEW;
    settings.UseSrgbFramebuffer = true;
}

void OvrApp::LeavingVrMode() {
}

void OvrApp::EnteredVrMode(const ovrIntentType intentType, const char *intentFromPackage, const char *intentJSON, const char *intentURI) {
    OVR_UNUSED(intentFromPackage);
    OVR_UNUSED(intentJSON);
    OVR_UNUSED(intentURI);

    if (intentType == INTENT_LAUNCH) {

        OVR_LOG("INIT VRVB EMULATOR");
        //OVR_LOG("INIT VRVB EMULATOR");

        // used to get the battery level
        java = app->GetJava();
        clsData = java->Env->GetObjectClass(java->ActivityObject);

        FontManager::Init(MENU_WIDTH, MENU_HEIGHT);
        FontManager::LoadFontFromAssets(app, &fontHeader, "apk:///assets/fonts/VirtualLogo.ttf", 65);
        FontManager::LoadFont(&fontMenu, "/system/fonts/Roboto-Regular.ttf", 24);
        FontManager::LoadFont(&fontList, "/system/fonts/Roboto-Regular.ttf", 22);
        FontManager::LoadFont(&fontBottom, "/system/fonts/Roboto-Bold.ttf", 22);
        FontManager::LoadFont(&fontVersion, "/system/fonts/Roboto-Regular.ttf", 20);
        FontManager::LoadFont(&fontSlot, "/system/fonts/Roboto-Regular.ttf", 26);
        FontManager::LoadFont(&fontBattery, "/system/fonts/Roboto-Bold.ttf", 23);
        FontManager::LoadFont(&fontTime, "/system/fonts/Roboto-Bold.ttf", 25);
        FontManager::CloseFontLoader();

        textureBackgroundId = TextureLoader::Load(app, "apk:///assets/background2.dds");

        textureHeaderIconId = TextureLoader::Load(app, "apk:///assets/header_icon.dds");
        textureGbIconId = TextureLoader::Load(app, "apk:///assets/gb_cartridge.dds");
        textureVbIconId = TextureLoader::Load(app, "apk:///assets/vb_cartridge.dds");

        textureGbcIconId = TextureLoader::Load(app, "apk:///assets/gbc_cartridge.dds");
        textureSaveIconId = TextureLoader::Load(app, "apk:///assets/save_icon.dds");
        textureLoadIconId = TextureLoader::Load(app, "apk:///assets/load_icon.dds");
        textureResumeId = TextureLoader::Load(app, "apk:///assets/resume_icon.dds");
        textureSettingsId = TextureLoader::Load(app, "apk:///assets/settings_icon.dds");
        texuterLeftRightIconId = TextureLoader::Load(app, "apk:///assets/leftright_icon.dds");
        textureUpDownIconId = TextureLoader::Load(app, "apk:///assets/updown_icon.dds");
        textureResetIconId = TextureLoader::Load(app, "apk:///assets/reset_icon.dds");
        textureSaveSlotIconId = TextureLoader::Load(app, "apk:///assets/save_slot_icon.dds");
        textureLoadRomIconId = TextureLoader::Load(app, "apk:///assets/rom_list_icon.dds");
        textureMoveIconId = TextureLoader::Load(app, "apk:///assets/move_icon.dds");
        textureBackIconId = TextureLoader::Load(app, "apk:///assets/back_icon.dds");
        textureDistanceIconId = TextureLoader::Load(app, "apk:///assets/distance_icon.dds");
        textureResetViewIconId = TextureLoader::Load(app, "apk:///assets/reset_view_icon.dds");
        textureScaleIconId = TextureLoader::Load(app, "apk:///assets/scale_icon.dds");
        textureMappingIconId = TextureLoader::Load(app, "apk:///assets/mapping_icon.dds");
        texturePaletteIconId = TextureLoader::Load(app, "apk:///assets/palette_icon.dds");
        textureButtonAIconId = TextureLoader::Load(app, "apk:///assets/button_a_icon.dds");
        textureButtonBIconId = TextureLoader::Load(app, "apk:///assets/button_b_icon.dds");
        textureButtonXIconId = TextureLoader::Load(app, "apk:///assets/button_x_icon.dds");
        textureButtonYIconId = TextureLoader::Load(app, "apk:///assets/button_y_icon.dds");
        textureFollowHeadIconId = TextureLoader::Load(app, "apk:///assets/follow_head_icon.dds");
        textureDMGIconId = TextureLoader::Load(app, "apk:///assets/force_dmg_icon.dds");
        textureExitIconId = TextureLoader::Load(app, "apk:///assets/exit_icon.dds");
        threedeeIconId = TextureLoader::Load(app, "apk:///assets/3d_icon.dds");
        twodeeIconId = TextureLoader::Load(app, "apk:///assets/2d_icon.dds");

        textureIpdIconId = TextureLoader::Load(app, "apk:///assets/ipd_icon.dds");

        mappingLeftDownId = TextureLoader::Load(app, "apk:///assets/icons/mapping/left_down.dds");
        mappingLeftUpId = TextureLoader::Load(app, "apk:///assets/icons/mapping/left_up.dds");
        mappingLeftLeftId = TextureLoader::Load(app, "apk:///assets/icons/mapping/left_left.dds");
        mappingLeftRightId = TextureLoader::Load(app, "apk:///assets/icons/mapping/left_right.dds");

        mappingRightDownId = TextureLoader::Load(app, "apk:///assets/icons/mapping/right_down.dds");
        mappingRightUpId = TextureLoader::Load(app, "apk:///assets/icons/mapping/right_up.dds");
        mappingRightLeftId = TextureLoader::Load(app, "apk:///assets/icons/mapping/right_left.dds");
        mappingRightRightId = TextureLoader::Load(app, "apk:///assets/icons/mapping/right_right.dds");

        mappingStartId = TextureLoader::Load(app, "apk:///assets/icons/mapping/start.dds");
        mappingSelectId = TextureLoader::Load(app, "apk:///assets/icons/mapping/select.dds");

        mappingTriggerLeft = TextureLoader::Load(app, "apk:///assets/icons/mapping/trigger_left.dds");
        mappingTriggerRight = TextureLoader::Load(app, "apk:///assets/icons/mapping/trigger_right.dds");

        textureWhiteId = TextureLoader::CreateWhiteTexture();

        OVR_LOG("INIT VRVB EMULATOR1");
        // init audio
        OpenSLWrap_Init();

        OVR_LOG("INIT VRVB EMULATOR2");
        DrawHelper::Init(MENU_WIDTH, MENU_HEIGHT);

        Emulator::ResetButtonMapping();

        OVR_LOG("INIT VRVB EMULATOR Load Settings");
        LoadSettings();

        OVR_LOG("INIT VRVB EMULATOR Rom Folder");
        romFolderPath = appStoragePath;
        romFolderPath += Emulator::romFolderPath;

        OVR_LOG("INIT VRVB EMULATOR Init Emulator");
        Emulator::Init(appStoragePath);

        OVR_LOG("Scan dir");
        ScanDirectory();

        MenuGo::SetUpMenu();

        CreateScreen();

        // number of supported refreshrates
        int refreshRateCount =
                vrapi_GetSystemPropertyInt(java,
                                           VRAPI_SYS_PROP_NUM_SUPPORTED_DISPLAY_REFRESH_RATES);
        float *supportedRefreshRates = new float[refreshRateCount];
        vrapi_GetSystemPropertyFloatArray(java,
                                          VRAPI_SYS_PROP_SUPPORTED_DISPLAY_REFRESH_RATES,
                                          supportedRefreshRates,
                                          refreshRateCount);

        // set the refresh rate to the one requested by the emulator if it is available
        for (int i = 0; i < refreshRateCount; ++i) {
            if (supportedRefreshRates[i] == DisplayRefreshRate) {
                vrapi_SetDisplayRefreshRate(app->GetOvrMobile(), DisplayRefreshRate);
                OVR_LOG("Set to %f", DisplayRefreshRate);
                break;
            }
        }

        OVR_LOG("Done stuff 123");
        //const ovrJava *java = app->GetJava();
        //SoundEffectContext = new ovrSoundEffectContext(*java->Env, java->ActivityObject);
        //SoundEffectContext->Initialize(&app->GetFileSys());

        SoundEffectPlayer = new OvrGuiSys::ovrDummySoundEffectPlayer();

        Locale = ovrLocale::Create(*java->Env, java->ActivityObject, "default");

        String fontName;
        GetLocale().GetString("@string/font_name", "efigs.fnt", fontName);
        GuiSys->Init(this->app, *SoundEffectPlayer, fontName.ToCStr(), &app->GetDebugLines());

    } else if (intentType == INTENT_NEW) {
    }
}

ovrFrameResult OvrApp::Frame(const ovrFrameInput &vrFrame) {
    return MenuGo::Update(app, vrFrame);
}


