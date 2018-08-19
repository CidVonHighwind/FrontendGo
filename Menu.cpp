#include "Menu.h"
#include <sstream>
#include <dirent.h>
#include <fstream>

#include "Audio/OpenSLWrap.h"
#include "DrawHelper.h"
#include "FontMaster.h"
#include "LayerBuilder.h"
#include "TextureLoader.h"
#include "MenuHelper.h"
#include "Emulator.h"
#include "Global.h"

#define OPEN_MENU_SPEED 0.1f
#define MENU_TRANSITION_SPEED 0.1f

#define MAX_SAVE_SLOTS 10
#define MoveSpeed 0.01 // 0.00390625f
#define ZoomSpeed 0.03125f
#define MIN_DISTANCE 0.5f
#define MAX_DISTANCE 5.5f

using namespace MenuGo;
using namespace OVR;

void SaveSettings();

int batteryColorCount = 5;
ovrVector4f BatteryColors[] = {{0.745f, 0.114f, 0.176f, 1.0f},
                               {0.92f,  0.361f, 0.176f, 1.0f},
                               {0.976f, 0.69f,  0.255f, 1.0f},
                               {0.545f, 0.769f, 0.247f, 1.0f},
                               {0.545f, 0.769f, 0.247f, 1.0f},
                               {0.0f,   0.78f,  0.078f, 1.0f},
                               {0.0f,   0.78f,  0.078f, 1.0f}};

// saved variables
bool showExitDialog = false;
bool resetView = false;
bool SwappSelectBackButton = false;

uint SelectButton = BUTTON_A;
uint BackButton = BUTTON_B;

float transitionPercentage = 1.0f;
uint uButtonState, uLastButtonState, buttonState, lastButtonState;

int button_mapping_menu;
uint button_mapping_menu_index = 2; // X

std::string MapButtonStr[] = {"A", "B", "X", "Y", "Start", "Back", "Select", "Menu",
                              "Right Trigger", "Left Trigger", "DPad-Up", "DPad-Down",
                              "DPad-Left", "DPad-Right", "LStick-Up", "LStick-Down",
                              "LStick-Left",
                              "LStick-Right", "RStick-Up", "RStick-Down", "RStick-Left",
                              "RStick-Right"};

std::string strMove[] = {"Follow Head: Yes", "Follow Head: No"};

int strVersionWidth;

int batteryLevel, batter_string_width, time_string_width;
std::string time_string, battery_string;

int ButtonMappingIndex = 0;
bool UpdateMapping = false;
float MappingOverlayPercentage;

ovrTextureSwapChain *MenuSwapChain;
GLuint MenuFrameBuffer = 0;

bool isTransitioning;
int transitionMoveDir = 1;
float transitionState = 1;

uint *remapButton;

void (*updateMappingText)() = nullptr;

uint possibleMappingIndices;

Menu *nextMenu, *currentBottomBar;
Menu romSelectionMenu, mainMenu, settingsMenu, buttonMenuMapMenu, buttonEmulatorMapMenu, bottomBar,
        buttonMappingOverlay, moveMenu;

MenuButton *mappedButton;
MenuButton *backHelp, *menuHelp, *selectHelp;
MenuButton *yawButton, *pitchButton, *rollButton, *scaleButton, *distanceButton;
MenuButton *slotButton;
MenuButton *menuMappingButton;
std::vector<MenuButton> buttonMapping;

MenuLabel *mappingButtonLabel, *possibleMappingLabel;

Menu *currentMenu;

const int MENU_WIDTH = 640;
const int MENU_HEIGHT = 576;
const int HEADER_HEIGHT = 75;
const int BOTTOM_HEIGHT = 30;

int menuItemSize;
int saveSlot = 0;

bool menuOpen = true;
bool loadedRom = false;
bool followHead = false;

jmethodID getVal;
//VBEmulator *Emulator;

template<typename T>
std::string to_string(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

void StartTransition(Menu *next, int dir) {
    if (isTransitioning) return;
    isTransitioning = true;
    transitionMoveDir = dir;
    nextMenu = next;
}

void OnClickResumGame(MenuItem *item) {
    LOG("Pressed RESUME GAME");
    if (loadedRom) menuOpen = false;
}

void OnClickResetGame(MenuItem *item) {
    LOG("RESET GAME");
    if (loadedRom) {
        Emulator::ResetGame();
        menuOpen = false;
    }
}

void OnClickSaveGame(MenuItem *item) {
    LOG("on click save game");
    if (loadedRom) {
        Emulator::SaveState(saveSlot);
        menuOpen = false;
    }
}

void OnClickLoadGame(MenuItem *item) {
    if (loadedRom) {
        Emulator::LoadState(saveSlot);
        menuOpen = false;
    }
}

void OnClickLoadRomGame(MenuItem *item) { StartTransition(&romSelectionMenu, -1); }

void OnClickSettingsGame(MenuItem *item) { StartTransition(&settingsMenu, 1); }

void OnClickResetView(MenuItem *item) { resetView = true; }

void OnClickBackAndSave(MenuItem *item) {
    StartTransition(&mainMenu, -1);
    SaveSettings();
}

void OnBackPressedRomList() { StartTransition(&mainMenu, 1); }

void OnBackPressedSettings() {
    StartTransition(&mainMenu, -1);
    SaveSettings();
}

void OnClickBackMove(MenuItem *item) {
    StartTransition(&settingsMenu, -1);
}

void ChangeSaveSlot(MenuItem *item, int dir) {
    saveSlot += dir;
    if (saveSlot < 0) saveSlot = MAX_SAVE_SLOTS - 1;
    if (saveSlot >= MAX_SAVE_SLOTS) saveSlot = 0;
    Emulator::UpdateStateImage(saveSlot);
    ((MenuButton *) item)->Text = "Save Slot: " + to_string(saveSlot);
}

void OnClickSaveSlotLeft(MenuItem *item) {
    ChangeSaveSlot(item, -1);
    SaveSettings();
}

void OnClickSaveSlotRight(MenuItem *item) {
    ChangeSaveSlot(item, 1);
    SaveSettings();
}

void SwapButtonSelectBack(MenuItem *item) {
    SwappSelectBackButton = !SwappSelectBackButton;
    ((MenuButton *) item)->Text = "Swap Select and Back: ";
    ((MenuButton *) item)->Text.append((SwappSelectBackButton ? "Yes" : "No"));

    SelectButton = SwappSelectBackButton ? BUTTON_B : BUTTON_A;
    BackButton = SwappSelectBackButton ? BUTTON_A : BUTTON_B;

    selectHelp->IconId = SwappSelectBackButton ? textureButtonBIconId : textureButtonAIconId;
    backHelp->IconId = SwappSelectBackButton ? textureButtonAIconId : textureButtonBIconId;
}

uint GetPressedButton(uint &_buttonState, uint &_lastButtonState) {
    return _buttonState & ~_lastButtonState;
}

void MoveMenuButtonMapping(MenuItem *item, int dir) {
    button_mapping_menu_index += dir;
    if (button_mapping_menu_index > 3) button_mapping_menu_index = 2;

    button_mapping_menu = 0x1 << button_mapping_menu_index;
    ((MenuButton *) item)->Text = "menu mapped to: " + MapButtonStr[button_mapping_menu_index];

    menuHelp->IconId =
            button_mapping_menu_index == 2 ? textureButtonXIconId : textureButtonYIconId;
}

// mapping functions
void UpdateButtonMappingText(MenuItem *item) {
    LOG("Update mapping text for %i", item->Tag);
    ((MenuButton *) item)->Text =
            "mapped to: " + MapButtonStr[Emulator::button_mapping_index[item->Tag]];
}

void UpdateMenuMapping() {
    button_mapping_menu = 0x1 << button_mapping_menu_index;
    mappedButton->Text = "menu mapped to: " + MapButtonStr[button_mapping_menu_index];
}

void UpdateEmulatorMapping() {
    Emulator::UpdateButtonMapping();
    mappedButton->Text = "mapped to: " + MapButtonStr[*remapButton];
}

void OnClickChangeMenuButtonLeft(MenuItem *item) { MoveMenuButtonMapping(item, 1); }

void OnClickChangeMenuButtonRight(MenuItem *item) { MoveMenuButtonMapping(item, 1); }

void OnClickChangeMenuButtonEnter(MenuItem *item) {
    UpdateMapping = true;
    remapButton = &button_mapping_menu_index;
    mappedButton = menuMappingButton;
    updateMappingText = UpdateMenuMapping;

    possibleMappingIndices = BUTTON_X | BUTTON_Y;

    mappingButtonLabel->SetText("Menu Button");
    possibleMappingLabel->SetText("(X, Y)");
}

void OnClickChangeButtonMappingEnter(MenuItem *item) {
    UpdateMapping = true;
    remapButton = &Emulator::button_mapping_index[item->Tag];
    mappedButton = &buttonMapping.at(item->Tag);
    updateMappingText = UpdateEmulatorMapping;

    // buttons from BUTTON_A to BUTTON_RSTICK_RIGHT
    possibleMappingIndices =
            BUTTON_TOUCH - 1;// 4194303;// BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y;

    mappingButtonLabel->SetText(MapButtonStr[item->Tag]);
    possibleMappingLabel->SetText("(A, B, X, Y,...)");
}

void UpdateButtonMapping(MenuItem *item, uint &_buttonState, uint &_lastButtonState) {
    if (UpdateMapping) {
        uint newButtonState = GetPressedButton(_buttonState, _lastButtonState);

        LOG("button %i", _buttonState);

        if (newButtonState & possibleMappingIndices) {
            int buttonIndex = 0;
            while (!(newButtonState & 0x1)) {
                newButtonState >>= 1;
                buttonIndex++;
            }

            LOG("mapped to %i", buttonIndex);
            UpdateMapping = false;
            *remapButton = buttonIndex;
            updateMappingText();
        }

        _buttonState = 0;
        _lastButtonState = 0;
    }
}

void OnClickExit(MenuItem *item) {
    Emulator::SaveRam();
    showExitDialog = true;
}

void OnClickBackMainMenu() {
    if (loadedRom) menuOpen = false;
}

void OnClickFollowMode(MenuItem *item) {
    followHead = !followHead;
    ((MenuButton *) item)->Text = strMove[followHead ? 0 : 1];
}

void OnClickMoveScreen(MenuItem *item) { StartTransition(&moveMenu, 1); }

void OnClickMenuMappingScreen(MenuItem *item) { StartTransition(&buttonMenuMapMenu, 1); }

void OnClickEmulatorMappingScreen(MenuItem *item) {
    StartTransition(&buttonEmulatorMapMenu, 1);
}

void OnBackPressedMove() {
    StartTransition(&settingsMenu, -1);
}

float ToDegree(float radian) { return (int) (180.0 / VRAPI_PI * radian * 10) / 10.0f; }

void MoveYaw(MenuItem *item, float dir) {
    LayerBuilder::screenYaw -= dir;
    ((MenuButton *) item)->Text = "Yaw: " + to_string(ToDegree(LayerBuilder::screenYaw)) + "°";
}

void MovePitch(MenuItem *item, float dir) {
    LayerBuilder::screenPitch -= dir;
    ((MenuButton *) item)->Text =
            "Pitch: " + to_string(ToDegree(LayerBuilder::screenPitch)) + "°";
}

void ChangeDistance(MenuItem *item, float dir) {
    LayerBuilder::radiusMenuScreen -= dir;

    if (LayerBuilder::radiusMenuScreen < MIN_DISTANCE)
        LayerBuilder::radiusMenuScreen = MIN_DISTANCE;
    if (LayerBuilder::radiusMenuScreen > MAX_DISTANCE)
        LayerBuilder::radiusMenuScreen = MAX_DISTANCE;

    ((MenuButton *) item)->Text = "Distance: " + to_string(LayerBuilder::radiusMenuScreen);
}

void ChangeScale(MenuItem *item, float dir) {
    LayerBuilder::screenSize -= dir;

    if (LayerBuilder::screenSize < 0.05f) LayerBuilder::screenSize = 0.05f;
    if (LayerBuilder::screenSize > 20.0f) LayerBuilder::screenSize = 20.0f;

    ((MenuButton *) item)->Text = "Scale: " + to_string(LayerBuilder::screenSize);
}

void MoveRoll(MenuItem *item, float dir) {
    LayerBuilder::screenRoll -= dir;
    ((MenuButton *) item)->Text =
            "Roll: " + to_string(ToDegree(LayerBuilder::screenRoll)) + "°";
}

void OnClickResetEmulatorMapping(MenuItem *item) {
    Emulator::RestButtonMapping();

    for (int i = 0; i < Emulator::buttonCount; ++i) {
        UpdateButtonMappingText(&buttonMapping.at(i));
    }
}

void OnClickResetViewSettings(MenuItem *item) {
    LayerBuilder::ResetValues();

    // updates the visible values
    MoveYaw(yawButton, 0);
    MovePitch(pitchButton, 0);
    MoveRoll(rollButton, 0);
    ChangeDistance(distanceButton, 0);
    ChangeScale(scaleButton, 0);
}

void OnClickYaw(MenuItem *item) {
    LayerBuilder::screenYaw = 0;
    MoveYaw(yawButton, 0);
}

void OnClickPitch(MenuItem *item) {
    LayerBuilder::screenPitch = 0;
    MovePitch(pitchButton, 0);
}

void OnClickRoll(MenuItem *item) {
    LayerBuilder::screenRoll = 0;
    MoveRoll(rollButton, 0);
}

void OnClickDistance(MenuItem *item) {
    LayerBuilder::radiusMenuScreen = 0.75f;
    ChangeDistance(distanceButton, 0);
}

void OnClickScale(MenuItem *item) {
    LayerBuilder::screenSize = 1.0f;
    ChangeScale(scaleButton, 0);
}

void OnClickMoveScreenYawLeft(MenuItem *item) { MoveYaw(item, MoveSpeed); }

void OnClickMoveScreenYawRight(MenuItem *item) { MoveYaw(item, -MoveSpeed); }

void OnClickMoveScreenPitchLeft(MenuItem *item) { MovePitch(item, -MoveSpeed); }

void OnClickMoveScreenPitchRight(MenuItem *item) { MovePitch(item, MoveSpeed); }

void OnClickMoveScreenRollLeft(MenuItem *item) { MoveRoll(item, -MoveSpeed); }

void OnClickMoveScreenRollRight(MenuItem *item) { MoveRoll(item, MoveSpeed); }

void OnClickMoveScreenDistanceLeft(MenuItem *item) { ChangeDistance(item, ZoomSpeed); }

void OnClickMoveScreenDistanceRight(MenuItem *item) {
    ChangeDistance(item, -ZoomSpeed);
}

void OnClickMoveScreenScaleLeft(MenuItem *item) { ChangeScale(item, MoveSpeed); }

void OnClickMoveScreenScaleRight(MenuItem *item) { ChangeScale(item, -MoveSpeed); }

void ResetMenuState() {
    SaveSettings();
    saveSlot = 0;
    ChangeSaveSlot(slotButton, 0);
    currentMenu = &mainMenu;
    menuOpen = false;
    loadedRom = true;
}

void MenuGo::SetUpMenu() {

    getVal = java->Env->GetMethodID(clsData, "GetBatteryLevel", "()I");

    LOG("Set up Menu");
    int bigGap = 10;
    int smallGap = 5;

    LOG("got emulator");

    menuItemSize = (fontMenu.FontSize + 4);
    strVersionWidth = GetWidth(fontVersion, STR_VERSION);

    {
        LOG("Set up Rom Selection Menu");
        Emulator::InitRomSelectionMenu(0, 0, romSelectionMenu);

        romSelectionMenu.CurrentSelection = 0;

        romSelectionMenu.BackPress = OnBackPressedRomList;
        romSelectionMenu.Init();
    }

    {
        menuHelp = new MenuButton(&fontBottom,
                                  button_mapping_menu_index == 2 ? textureButtonXIconId
                                                                 : textureButtonYIconId,
                                  "Close Menu", 7,
                                  MENU_HEIGHT - BOTTOM_HEIGHT,
                                  BOTTOM_HEIGHT,
                                  nullptr,
                                  nullptr,
                                  nullptr);
        menuHelp->Color = MenuBottomColor;
        backHelp = new MenuButton(&fontBottom,
                                  SwappSelectBackButton ? textureButtonAIconId
                                                        : textureButtonBIconId,
                                  "Back",
                                  MENU_WIDTH - 210,
                                  MENU_HEIGHT - BOTTOM_HEIGHT,
                                  BOTTOM_HEIGHT,
                                  nullptr,
                                  nullptr,
                                  nullptr);
        backHelp->Color = MenuBottomColor;
        selectHelp = new MenuButton(&fontBottom,
                                    SwappSelectBackButton ? textureButtonBIconId
                                                          : textureButtonAIconId,
                                    "Select",
                                    MENU_WIDTH - 110,
                                    MENU_HEIGHT - BOTTOM_HEIGHT,
                                    BOTTOM_HEIGHT,
                                    nullptr,
                                    nullptr,
                                    nullptr);
        selectHelp->Color = MenuBottomColor;

        bottomBar.MenuItems.push_back(backHelp);
        bottomBar.MenuItems.push_back(selectHelp);
        bottomBar.MenuItems.push_back(menuHelp);
        currentBottomBar = &bottomBar;
    }

    int posX = 20;
    int posY = HEADER_HEIGHT + 20;

    // main menu page
    {
        mainMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureResumeId,
                                                    "Resume Game",
                                                    posX,
                                                    posY,
                                                    OnClickResumGame,
                                                    nullptr,
                                                    nullptr));
        mainMenu.MenuItems.push_back(
                new MenuButton(&fontMenu, textureResetIconId, "Reset Game", posX,
                               posY += menuItemSize, OnClickResetGame, nullptr,
                               nullptr));
        slotButton =
                new MenuButton(&fontMenu, textureSaveSlotIconId, "", posX,
                               posY += menuItemSize + 10,
                               OnClickSaveSlotRight, OnClickSaveSlotLeft, OnClickSaveSlotRight);

        slotButton->ScrollTimeH = 10;
        ChangeSaveSlot(slotButton, 0);
        mainMenu.MenuItems.push_back(slotButton);
        mainMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureSaveIconId,
                                                    "Save",
                                                    posX,
                                                    posY += menuItemSize,
                                                    OnClickSaveGame,
                                                    nullptr,
                                                    nullptr));
        mainMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureLoadIconId,
                                                    "Load",
                                                    posX,
                                                    posY += menuItemSize,
                                                    OnClickLoadGame,
                                                    nullptr,
                                                    nullptr));

        mainMenu.MenuItems.push_back(
                new MenuButton(&fontMenu, textureLoadRomIconId, "Load Rom", posX,
                               posY += menuItemSize + 10, OnClickLoadRomGame,
                               nullptr, nullptr));
        mainMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureResetViewIconId,
                                                    "Reset View",
                                                    posX,
                                                    posY += menuItemSize,
                                                    OnClickResetView,
                                                    nullptr,
                                                    nullptr));
        mainMenu.MenuItems.push_back(
                new MenuButton(&fontMenu, textureSettingsId, "Settings", posX,
                               posY += menuItemSize, OnClickSettingsGame,
                               nullptr,
                               nullptr));
        mainMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureExitIconId,
                                                    "Exit",
                                                    posX,
                                                    posY += menuItemSize + 10,
                                                    OnClickExit,
                                                    nullptr,
                                                    nullptr));

        LOG("Set up Main Menu");
        Emulator::InitMainMenu(posX, posY, mainMenu);

        mainMenu.Init();
        mainMenu.BackPress = OnClickBackMainMenu;
    }

    // settings page
    {
        posY = HEADER_HEIGHT + 20;

        settingsMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                        textureMappingIconId,
                                                        "Menu Button Mapping",
                                                        posX,
                                                        posY,
                                                        OnClickMenuMappingScreen,
                                                        nullptr,
                                                        nullptr));
        settingsMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                        textureMappingIconId,
                                                        "Emulator Button Mapping",
                                                        posX,
                                                        posY += menuItemSize,
                                                        OnClickEmulatorMappingScreen,
                                                        nullptr,
                                                        nullptr));
        settingsMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                        textureMoveIconId,
                                                        "Move Screen",
                                                        posX,
                                                        posY += menuItemSize,
                                                        OnClickMoveScreen,
                                                        nullptr,
                                                        nullptr));
        settingsMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                        textureFollowHeadIconId,
                                                        strMove[followHead ? 0 : 1],
                                                        posX,
                                                        posY += menuItemSize + bigGap,
                                                        OnClickFollowMode,
                                                        OnClickFollowMode,
                                                        OnClickFollowMode));

        LOG("Set up Settings Menu");
        Emulator::InitSettingsMenu(posX, posY, settingsMenu);

        settingsMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                        textureBackIconId,
                                                        "Save and Back",
                                                        posX,
                                                        posY += menuItemSize + bigGap,
                                                        OnClickBackAndSave,
                                                        nullptr,
                                                        nullptr));

        settingsMenu.MenuItems.push_back(new MenuLabel(&fontVersion, STR_VERSION,
                                                       MENU_WIDTH - 70,
                                                       MENU_HEIGHT - BOTTOM_HEIGHT - 50 + 10,
                                                       70, 50, textColorVersion));

        settingsMenu.BackPress = OnBackPressedSettings;
        settingsMenu.Init();
    }

    // menu button mapping
    {
        posY = HEADER_HEIGHT + 20;

        MenuButton *swapButton =
                new MenuButton(&fontMenu, textureLoadRomIconId, "", posX, posY,
                               SwapButtonSelectBack,
                               SwapButtonSelectBack, SwapButtonSelectBack);
        swapButton->UpdateFunction = UpdateButtonMapping;

        menuMappingButton =
                new MenuButton(&fontMenu,
                               textureLoadRomIconId,
                               "",
                               posX,
                               posY += menuItemSize,
                               OnClickChangeMenuButtonEnter,
                               OnClickChangeMenuButtonLeft,
                               OnClickChangeMenuButtonRight);
        buttonMenuMapMenu.MenuItems.push_back(swapButton);
        buttonMenuMapMenu.MenuItems.push_back(menuMappingButton);

        SwapButtonSelectBack(swapButton);
        SwapButtonSelectBack(swapButton);

        buttonMenuMapMenu.MenuItems.push_back(
                new MenuButton(&fontMenu, textureBackIconId, "Back", posX,
                               posY += menuItemSize + bigGap,
                               OnClickBackMove,
                               nullptr, nullptr));
        buttonMenuMapMenu.BackPress = OnBackPressedMove;
        buttonMenuMapMenu.Init();
    }

    // emulator button mapping
    {
        posY = HEADER_HEIGHT + 10;

        for (int i = 0; i < Emulator::buttonCount; ++i) {
            LOG("Set up mapping for %i", i);

            MenuButton *newButton = new MenuButton(&fontMenu, *Emulator::button_icons[i], "abc",
                                                   posX,
                                                   posY, OnClickChangeButtonMappingEnter,
                                                   nullptr,
                                                   nullptr);

            posY += menuItemSize;
            newButton->Tag = i;

            UpdateButtonMappingText(newButton);

            if (i == 0)
                newButton->UpdateFunction = UpdateButtonMapping;

            buttonMapping.push_back(*newButton);
        }

        MoveMenuButtonMapping(menuMappingButton, 0);

        // button mapping page
        for (int i = 0; i < Emulator::buttonCount; ++i)
            buttonEmulatorMapMenu.MenuItems.push_back(&buttonMapping.at(i));

        buttonEmulatorMapMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                                 textureResetViewIconId,
                                                                 "Reset Mapping", posX,
                                                                 posY += bigGap,
                                                                 OnClickResetEmulatorMapping,
                                                                 nullptr, nullptr));
        buttonEmulatorMapMenu.MenuItems.push_back(
                new MenuButton(&fontMenu, textureBackIconId, "Back", posX,
                               posY += menuItemSize - 3, OnClickBackMove,
                               nullptr, nullptr));
        buttonEmulatorMapMenu.BackPress = OnBackPressedMove;
        buttonEmulatorMapMenu.Init();
    }

    {
        // button mapping overlay
        buttonMappingOverlay.MenuItems.push_back(new MenuImage(textureWhiteId,
                                                               0,
                                                               0,
                                                               MENU_WIDTH,
                                                               MENU_HEIGHT,
                                                               {0.0f, 0.0f, 0.0f, 0.8f}));
        int overlayWidth = 250;
        int overlayHeight = 80;
        int margin = 15;
        buttonMappingOverlay.MenuItems.push_back(new MenuImage(textureWhiteId,
                                                               MENU_WIDTH / 2 -
                                                               overlayWidth / 2,
                                                               MENU_HEIGHT / 2 -
                                                               overlayHeight / 2
                                                               - margin,
                                                               overlayWidth,
                                                               overlayHeight + margin * 2,
                                                               {0.05f, 0.05f, 0.05f, 0.3f}));

        mappingButtonLabel = new MenuLabel(&fontMenu,
                                           "A Button",
                                           MENU_WIDTH / 2 - overlayWidth / 2,
                                           MENU_HEIGHT / 2 - overlayHeight / 2,
                                           overlayWidth,
                                           overlayHeight / 3,
                                           {0.9f, 0.9f, 0.9f, 0.9f});
        possibleMappingLabel = new MenuLabel(&fontMenu,
                                             "(A, B, X, Y)",
                                             MENU_WIDTH / 2 - overlayWidth / 2,
                                             MENU_HEIGHT / 2 + overlayHeight / 2
                                             - overlayHeight / 3,
                                             overlayWidth,
                                             overlayHeight / 3,
                                             {0.9f, 0.9f, 0.9f, 0.9f});

        buttonMappingOverlay.MenuItems.push_back(mappingButtonLabel);
        buttonMappingOverlay.MenuItems.push_back(new MenuLabel(&fontMenu,
                                                               "Press Button",
                                                               MENU_WIDTH / 2 -
                                                               overlayWidth / 2,
                                                               MENU_HEIGHT / 2 -
                                                               overlayHeight / 2
                                                               + overlayHeight / 3,
                                                               overlayWidth,
                                                               overlayHeight / 3,
                                                               {0.9f, 0.9f, 0.9f, 0.9f}));
        buttonMappingOverlay.MenuItems.push_back(possibleMappingLabel);
    }

    // move menu page
    {
        posY = HEADER_HEIGHT + 20;
        yawButton = new MenuButton(&fontMenu, texuterLeftRightIconId, "", posX, posY,
                                   OnClickYaw,
                                   OnClickMoveScreenYawLeft, OnClickMoveScreenYawRight);
        yawButton->ScrollTimeH = 1;
        pitchButton =
                new MenuButton(&fontMenu, textureUpDownIconId, "", posX, posY += menuItemSize,
                               OnClickPitch,
                               OnClickMoveScreenPitchLeft, OnClickMoveScreenPitchRight);
        pitchButton->ScrollTimeH = 1;
        rollButton =
                new MenuButton(&fontMenu, textureResetIconId, "", posX, posY += menuItemSize,
                               OnClickRoll,
                               OnClickMoveScreenRollLeft, OnClickMoveScreenRollRight);
        rollButton->ScrollTimeH = 1;
        distanceButton = new MenuButton(&fontMenu,
                                        textureDistanceIconId,
                                        "",
                                        posX,
                                        posY += menuItemSize,
                                        OnClickDistance,
                                        OnClickMoveScreenDistanceLeft,
                                        OnClickMoveScreenDistanceRight);
        distanceButton->ScrollTimeH = 1;
        scaleButton =
                new MenuButton(&fontMenu, textureScaleIconId, "", posX, posY += menuItemSize,
                               OnClickScale,
                               OnClickMoveScreenScaleLeft, OnClickMoveScreenScaleRight);
        scaleButton->ScrollTimeH = 1;

        moveMenu.MenuItems.push_back(yawButton);
        moveMenu.MenuItems.push_back(pitchButton);
        moveMenu.MenuItems.push_back(rollButton);
        moveMenu.MenuItems.push_back(distanceButton);
        moveMenu.MenuItems.push_back(scaleButton);

        moveMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureResetViewIconId,
                                                    "Reset View",
                                                    posX,
                                                    posY += menuItemSize + smallGap,
                                                    OnClickResetViewSettings,
                                                    nullptr,
                                                    nullptr));
        moveMenu.MenuItems.push_back(new MenuButton(&fontMenu,
                                                    textureBackIconId,
                                                    "Back",
                                                    posX,
                                                    posY += menuItemSize + bigGap,
                                                    OnClickBackMove,
                                                    nullptr,
                                                    nullptr));
        moveMenu.BackPress = OnBackPressedMove;
        moveMenu.Init();
    }

    currentMenu = &romSelectionMenu;

    // updates the visible values
    MoveYaw(yawButton, 0);
    MovePitch(pitchButton, 0);
    MoveRoll(rollButton, 0);
    ChangeDistance(distanceButton, 0);
    ChangeScale(scaleButton, 0);
}

int UpdateBatteryLevel() {
    jint bLevel = java->Env->CallIntMethod(java->ActivityObject, getVal);
    int returnValue = (int) bLevel;
    return returnValue;
}

void CreateScreen() {
    // menu layer
    MenuSwapChain =
            vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888_sRGB,
                                         MENU_WIDTH, MENU_HEIGHT, 1, false);

    textureIdMenu = vrapi_GetTextureSwapChainHandle(MenuSwapChain, 0);
    glBindTexture(GL_TEXTURE_2D, textureIdMenu);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, MENU_WIDTH, MENU_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                    NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindTexture(GL_TEXTURE_2D, 0);

    // create hte framebuffer for the menu texture
    glGenFramebuffers(1, &MenuFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, MenuFrameBuffer);

    // Set "renderedTexture" as our colour attachement #0
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           (GLuint) textureIdMenu,
                           0);

    // Set the list of draw buffers.
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);  // "1" is the size of DrawBuffers

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    LOG("finished creating screens");
}

void SaveSettings() {
    std::ofstream saveFile(saveFilePath, std::ios::trunc | std::ios::binary);
    saveFile.write(reinterpret_cast<const char *>(&SAVE_FILE_VERSION), sizeof(int));

    Emulator::SaveEmulatorSettings(&saveFile);
    saveFile.write(reinterpret_cast<const char *>(&followHead), sizeof(bool));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenPitch), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenYaw), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenRoll), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::radiusMenuScreen),
                   sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenSize), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&button_mapping_menu_index), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&SwappSelectBackButton), sizeof(bool));

    saveFile.close();
    LOG("Saved Settings");
}

void LoadSettings() {
    std::ifstream loadFile(saveFilePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (loadFile.is_open()) {
        loadFile.seekg(0, std::ios::beg);

        int saveFileVersion = 0;
        loadFile.read((char *) &saveFileVersion, sizeof(int));

        // only load if the save versions are compatible
        if (saveFileVersion == SAVE_FILE_VERSION) {
            Emulator::LoadEmulatorSettings(&loadFile);
            loadFile.read((char *) &followHead, sizeof(bool));
            loadFile.read((char *) &LayerBuilder::screenPitch, sizeof(float));
            loadFile.read((char *) &LayerBuilder::screenYaw, sizeof(float));
            loadFile.read((char *) &LayerBuilder::screenRoll, sizeof(float));
            loadFile.read((char *) &LayerBuilder::radiusMenuScreen, sizeof(float));
            loadFile.read((char *) &LayerBuilder::screenSize, sizeof(float));
            loadFile.read((char *) &button_mapping_menu_index, sizeof(int));
            loadFile.read((char *) &SwappSelectBackButton, sizeof(bool));
        }

        // TODO: reset all loaded settings
        if (loadFile.fail())
            LOG("Failed Loading Settings");
        else
            LOG("Settings Loaded");

        loadFile.close();
    }

    button_mapping_menu = 0x1 << button_mapping_menu_index;
}

void ScanDirectory() {
    DIR *dir;
    struct dirent *ent;
    std::string strFullPath;

    if ((dir = opendir(romFolderPath.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            strFullPath = "";
            strFullPath.append(romFolderPath);
            strFullPath.append(ent->d_name);

            if (ent->d_type == 8) {
                std::string strFilename = ent->d_name;

                // check if the filetype is supported by the emulator
                bool supportedFile = false;
                for (int i = 0; i < Emulator::supportedFileNames.size(); ++i) {
                    if (strFilename.find(Emulator::supportedFileNames.at(i)) !=
                        std::string::npos) {
                        supportedFile = true;
                        break;
                    }
                }

                if (supportedFile) {
                    Emulator::AddRom(strFullPath, strFilename);
                }
            }
        }
        closedir(dir);

        Emulator::SortRomList();
    } else {
        LOG("could not open folder");
    }

    LOG("scanned directory");
}

// void OvrApp::LeavingVrMode() {}

void GetTimeString(std::string &timeString) {
    struct timespec res;
    clock_gettime(CLOCK_REALTIME, &res);
    time_t t = res.tv_sec;  // just in case types aren't the same
    tm tmv;
    localtime_r(&t, &tmv);  // populate tmv with local time info

    timeString.clear();
    if (tmv.tm_hour < 10) timeString.append("0");
    timeString.append(to_string(tmv.tm_hour));
    timeString.append(":");
    if (tmv.tm_min < 10) timeString.append("0");
    timeString.append(to_string(tmv.tm_min));

    time_string_width = FontManager::GetWidth(fontTime, timeString);
}

void GetBattryString(std::string &batteryString) {
    batteryLevel = UpdateBatteryLevel();
    batteryString.clear();
    batteryString.append(to_string(batteryLevel));
    batteryString.append("%");

    batter_string_width = FontManager::GetWidth(fontBattery, batteryString);
}

void DrawGUI() {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    glBindFramebuffer(GL_FRAMEBUFFER, MenuFrameBuffer);
    // Render on the whole framebuffer, complete from the lower left corner to the
    // upper right
    glViewport(0, 0, MENU_WIDTH, MENU_HEIGHT);

    glClearColor(MenuBackgroundColor.x, MenuBackgroundColor.y, MenuBackgroundColor.z,
                 MenuBackgroundColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw the backgroud image
    //DrawHelper::DrawTexture(textureBackgroundId, 0, 0, menuWidth, menuHeight,
    //                        {0.7f,0.7f,0.7f,1.0f}, 0.985f);

    // header
    DrawHelper::DrawTexture(textureWhiteId, 0, 0, MENU_WIDTH, HEADER_HEIGHT,
                            MenuBackgroundOverlayHeader, 1);
    DrawHelper::DrawTexture(textureWhiteId, 0, MENU_HEIGHT - BOTTOM_HEIGHT, MENU_WIDTH,
                            BOTTOM_HEIGHT,
                            MenuBackgroundOverlayColorLight, 1);

    // icon
    //DrawHelper::DrawTexture(textureHeaderIconId, 0, 0, 75, 75, headerTextColor, 1);

    FontManager::Begin();
    FontManager::RenderText(fontHeader, STR_HEADER, 15,
                            HEADER_HEIGHT / 2 - fontHeader.PHeight / 2 - fontHeader.PStart,
                            1.0f, headerTextColor, 1);

    // update the battery string
    int batteryWidth = 10;
    int maxHeight = fontBattery.PHeight + 1;
    int distX = 15;
    int distY = 2;
    int batteryPosY = HEADER_HEIGHT / 2 + distY + 3;

    // update the time string
    GetTimeString(time_string);
    FontManager::RenderText(fontTime,
                            time_string,
                            MENU_WIDTH - time_string_width - distX,
                            HEADER_HEIGHT / 2 - distY - fontBattery.FontSize +
                            fontBattery.PStart,
                            1.0f,
                            textColorBattery,
                            1);

    GetBattryString(battery_string);
    FontManager::RenderText(fontBattery, battery_string,
                            MENU_WIDTH - batter_string_width - batteryWidth - 7 - distX,
                            HEADER_HEIGHT / 2 + distY + 3, 1.0f,
                            textColorBattery, 1);

    // FontManager::RenderText(fontSmall, STR_VERSION, menuWidth - strVersionWidth - 7.0f,
    //                        HEADER_HEIGHT - 21, 1.0f, textColorVersion, 1);
    FontManager::Close();

    // draw battery
    DrawHelper::DrawTexture(textureWhiteId,
                            MENU_WIDTH - batteryWidth - distX - 2 - 2,
                            batteryPosY,
                            batteryWidth + 4,
                            maxHeight + 4,
                            BatteryBackgroundColor,
                            1);

    // calculate the battery color
    float colorState =
            ((batteryLevel * 10) % (1000 / (batteryColorCount))) /
            (float) (1000 / batteryColorCount);
    int currentColor = (int) (batteryLevel / (100.0f / batteryColorCount));
    ovrVector4f batteryColor = ovrVector4f {
            BatteryColors[currentColor].x * (1 - colorState)
            + BatteryColors[currentColor + 1].x * colorState,
            BatteryColors[currentColor].y * (1 - colorState)
            + BatteryColors[currentColor + 1].y * colorState,
            BatteryColors[currentColor].z * (1 - colorState)
            + BatteryColors[currentColor + 1].z * colorState,
            BatteryColors[currentColor].w * (1 - colorState)
            + BatteryColors[currentColor + 1].w * colorState
    };

    int height = (int) (batteryLevel / 100.0f * maxHeight);

    DrawHelper::DrawTexture(textureWhiteId, MENU_WIDTH - batteryWidth - distX - 2,
                            batteryPosY + 2 + maxHeight - height,
                            batteryWidth, height, batteryColor, 1);

    DrawMenu();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static inline ovrLayerProjection2 vbLayerProjection() {
    ovrLayerProjection2 layer = {};

    const ovrMatrix4f
            projectionMatrix = ovrMatrix4f_CreateProjectionFov(90.0f, 90.0f, 0.0f, 0.0f, 0.1f,
                                                               0.0f);
    const ovrMatrix4f
            texCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(
            &projectionMatrix);

    layer.Header.Type = VRAPI_LAYER_TYPE_PROJECTION2;
    layer.Header.Flags = 0;
    layer.Header.ColorScale.x = 1.0f;
    layer.Header.ColorScale.y = 1.0f;
    layer.Header.ColorScale.z = 1.0f;
    layer.Header.ColorScale.w = 1.0f;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;
    //layer.Header.SurfaceTextureObject = NULL;

    layer.HeadPose.Pose.Orientation.w = 1.0f;

    for (int i = 0; i < VRAPI_FRAME_LAYER_EYE_MAX; i++) {
        layer.Textures[i].TexCoordsFromTanAngles = texCoordsFromTanAngles;
        layer.Textures[i].TextureRect.x = -1.0f;
        layer.Textures[i].TextureRect.y = -1.0f;
        layer.Textures[i].TextureRect.width = 2.0f;
        layer.Textures[i].TextureRect.height = 2.0f;
    }

    return layer;
}

void UpdateCurrentMenu() {
    if (isTransitioning) {
        transitionState -= MENU_TRANSITION_SPEED;
        if (transitionState < 0) {
            transitionState = 1;
            isTransitioning = false;
            currentMenu = nextMenu;
        }
    } else {
        currentMenu->Update(buttonState, lastButtonState);
    }
}


ovrMatrix4f CenterEyeViewMatrix;


ovrFrameResult MenuGo::Update(App *app, const ovrFrameInput &vrFrame) {

    // time:
    // vrFrame.PredictedDisplayTimeInSeconds

    uLastButtonState = uButtonState;
    uButtonState = vrFrame.Input.buttonState;
    // bug fix
    uButtonState |= vrFrame.Input.sticks[1][0] < -0.5f ? BUTTON_RSTICK_LEFT : 0;
    uButtonState |= vrFrame.Input.sticks[1][0] > 0.5f ? BUTTON_RSTICK_RIGHT : 0;
    uButtonState |= vrFrame.Input.sticks[1][1] < -0.5f ? BUTTON_RSTICK_UP : 0;
    uButtonState |= vrFrame.Input.sticks[1][1] > 0.5f ? BUTTON_RSTICK_DOWN : 0;

    buttonState = uButtonState;
    lastButtonState = uLastButtonState;

    //LOG("Button State: %i %f, %f", vrFrame.Input.buttonState, vrFrame.Input.sticks[0][0],
    //    vrFrame.Input.sticks[1][0]);

    // TODO speed
    if (!menuOpen) {
        if (transitionPercentage > 0) transitionPercentage -= OPEN_MENU_SPEED;
        if (transitionPercentage < 0) transitionPercentage = 0;

        Emulator::Update(vrFrame, buttonState, lastButtonState);
    } else {
        if (transitionPercentage < 1) transitionPercentage += OPEN_MENU_SPEED;
        if (transitionPercentage > 1) transitionPercentage = 1;

        UpdateCurrentMenu();
    }

    // open/close menu
    if (buttonState & button_mapping_menu && !(lastButtonState & button_mapping_menu) &&
        loadedRom) {
        menuOpen = !menuOpen;
    }

    CenterEyeViewMatrix = vrapi_GetViewMatrixFromPose(&vrFrame.Tracking.HeadPose.Pose);

    //res.Surfaces.PushBack( ovrDrawSurface( &SurfaceDef ) );

    // Clear the eye buffers to 0 alpha so the overlay plane shows through.
    ovrFrameResult res;
    res.ClearColorBuffer = true;
    res.ClearColor = Vector4f(0.0f, 0.0f, 0.0f, 1.0f);
    res.FrameMatrices.CenterView = CenterEyeViewMatrix;

    res.FrameIndex = vrFrame.FrameNumber;
    res.DisplayTime = vrFrame.PredictedDisplayTimeInSeconds;
    res.SwapInterval = app->GetSwapInterval();

    res.FrameFlags = 0;
    res.LayerCount = 0;

    ovrLayerProjection2 &worldLayer = res.Layers[res.LayerCount++].Projection;
    worldLayer = vbLayerProjection();
    worldLayer.HeadPose = vrFrame.Tracking.HeadPose;

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        res.FrameMatrices.EyeView[eye] = vrFrame.Tracking.Eye[eye].ViewMatrix;
        // Calculate projection matrix using custom near plane value.
        res.FrameMatrices.EyeProjection[eye] =
                ovrMatrix4f_CreateProjectionFov(vrFrame.FovX, vrFrame.FovY, 0.0f, 0.0f, 1.0f,
                                                0.0f);

        worldLayer.Textures[eye].ColorSwapChain = vrFrame.ColorTextureSwapChain[eye];
        worldLayer.Textures[eye].SwapChainIndex = vrFrame.TextureSwapChainIndex;
        worldLayer.Textures[eye].TexCoordsFromTanAngles = vrFrame.TexCoordsFromTanAngles;
    }

    LayerBuilder::UpdateDirection(vrFrame);
    Emulator::DrawScreenLayer(res, vrFrame);

    if (transitionPercentage > 0) {
        // menu layer
        if (menuOpen) DrawGUI();

        float transitionP = sinf((transitionPercentage) * MATH_FLOAT_PIOVER2);

        res.Layers[res.LayerCount].Cylinder = LayerBuilder::BuildSettingsCylinderLayer(
                MenuSwapChain, MENU_WIDTH, MENU_HEIGHT, &vrFrame.Tracking, followHead,
                transitionP);

        res.Layers[res.LayerCount].Cylinder.Header.Flags |=
                VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
        res.Layers[res.LayerCount].Cylinder.Header.Flags |=
                VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

        res.Layers[res.LayerCount].Cylinder.Header.ColorScale = {transitionP, transitionP,
                                                                 transitionP,
                                                                 transitionP};
        res.Layers[res.LayerCount].Cylinder.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
        res.Layers[res.LayerCount].Cylinder.Header.DstBlend =
                VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

        res.LayerCount++;
    }

    if (showExitDialog) {
        app->ShowSystemUI(VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
        showExitDialog = false;
    }
    if (resetView) {
        app->RecenterYaw(false);
        resetView = false;
    }

    return res;
}

void MenuGo::DrawMenu() {
    // the
    float trProgressOut = ((transitionState - 0.35f) / 0.65f);
    float progressOut = sinf(trProgressOut * MATH_FLOAT_PIOVER2);
    if (trProgressOut < 0)
        trProgressOut = 0;

    float trProgressIn = (((1 - transitionState) - 0.35f) / 0.65f);
    float progressIn = sinf(trProgressIn * MATH_FLOAT_PIOVER2);
    if (transitionState < 0)
        trProgressIn = 0;

    int dist = 75;

    if (UpdateMapping) {
        MappingOverlayPercentage += 0.2f;
        if (MappingOverlayPercentage > 1)
            MappingOverlayPercentage = 1;
    } else {
        MappingOverlayPercentage -= 0.2f;
        if (MappingOverlayPercentage < 0)
            MappingOverlayPercentage = 0;
    }

    // draw the current menu
    currentMenu->Draw(-transitionMoveDir, 0, (1 - progressOut), dist, trProgressOut);
    // draw the next menu fading in
    if (isTransitioning)
        nextMenu->Draw(transitionMoveDir, 0, (1 - progressIn), dist, trProgressIn);

    // draw the bottom bar
    currentBottomBar->Draw(0, 0, 0, 0, 1);

    if (MappingOverlayPercentage) {
        buttonMappingOverlay.Draw(0,
                                  -1,
                                  (1 - sinf(MappingOverlayPercentage * MATH_FLOAT_PIOVER2)),
                                  dist,
                                  MappingOverlayPercentage);
    }

    /*
    DrawHelper::DrawTexture(textureWhiteId,
                            0,
                            200,
                            fontMenu.FontSize * 30,
                            fontMenu.FontSize * 8,
                            {0.0f, 0.0f, 0.0f, 1.0f},
                            1.0f);

    FontManager::Begin();
    FontManager::RenderFontImage(fontMenu, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
    FontManager::Close(); */

}