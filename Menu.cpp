#include "Menu.h"
#include <sstream>
#include <dirent.h>
#include <fstream>
#include <VrApi/Include/VrApi_Input.h>
#include <VrAppSupport/VrGUI/Src/VRMenuObject.h>
#include <VrAppSupport/VrGUI/Src/VRMenu.h>

#include "Audio/OpenSLWrap.h"
#include "DrawHelper.h"
#include "FontMaster.h"
#include "LayerBuilder.h"
#include "TextureLoader.h"
#include "MenuHelper.h"
#include "Emulator.h"
#include "Global.h"
#include "ButtonMapping.h"

#define OPEN_MENU_SPEED 0.1f
#define MENU_TRANSITION_SPEED 0.1f

#define MAX_SAVE_SLOTS 10
#define MoveSpeed 0.01 // 0.00390625f
#define ZoomSpeed 0.03125f
#define MIN_DISTANCE 0.5f
#define MAX_DISTANCE 5.5f

using namespace MenuGo;
using namespace OVR;


int batteryColorCount = 5;
ovrVector4f BatteryColors[] = {{0.745F, 0.114F, 0.176F, 1.0F},
                               {0.92F,  0.361F, 0.176F, 1.0F},
                               {0.976F, 0.69F,  0.255F, 1.0F},
                               {0.545F, 0.769F, 0.247F, 1.0F},
                               {0.545F, 0.769F, 0.247F, 1.0F},
                               {0.0F,   0.78F,  0.078F, 1.0F},
                               {0.0F,   0.78F,  0.078F, 1.0F}};

// saved variables
bool showExitDialog = false;
bool resetView = false;
bool SwappSelectBackButton = false;
//
//uint SelectButton = BUTTON_A;
//uint BackButton = BUTTON_B;

float transitionPercentage = 1.0F;

//uint uButtonState, uLastButtonState, buttonState, lastButtonState;

uint buttonStatesReal[3];
uint buttonStates[3];
uint lastButtonStates[3];

//int button_mapping_menu;
//uint button_mapping_menu_index = 2; // X

MappedButtons buttonMappingMenu;

MappedButton *remapButton;

// gamepad button names; ltouch button names; rtouch button names
std::string MapButtonStr[] = {"A", "B", "RThumb", "RBumper",
                              "X", "Y", "LThumb", "LBumper",
                              "Up", "Down", "Left", "Right",
                              "Enter", "Back", "GripTrigger", "Trigger", "Joystick",
                              "LStick-Up", "LStick-Down", "LStick-Left", "LStick-Right",
                              "RStick-Up", "RStick-Down", "RStick-Left", "RStick-Right",
                              "LTrigger", "RTrigger", "", "", "", "", "",

                              "LTouch-A", "LTouch-B", "LTouch-RThumb", "LTouch-RShoulder",
                              "LTouch-X", "LTouch-Y", "LTouch-LThumb", "LTouch-LShoulder",
                              "LTouch-Up", "LTouch-Down", "LTouch-Left", "LTouch-Right",
                              "LTouch-Enter", "LTouch-Back", "LTouch-GripTrigger", "LTouch-Trigger",
                              "LTouch-Joystick",
                              "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",

                              "RTouch-A", "RTouch-B", "RTouch-RThumb", "RTouch-RShoulder",
                              "RTouch-X", "RTouch-Y", "RTouch-LThumb", "RTouch-LShoulder",
                              "RTouch-Up", "RTouch-Down", "RTouch-Left", "RTouch-Right",
                              "RTouch-Enter", "RTouch-Back", "RTouch-GripTrigger", "RTouch-Trigger",
                              "RTouch-Joystick",
                              "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};

std::string strMove[] = {"Follow Head: Yes", "Follow Head: No"};

int strVersionWidth;

int batteryLevel, batter_string_width, time_string_width;
std::string time_string, battery_string;

int ButtonMappingIndex = 0;
bool UpdateMapping = false;
bool UpdateMappingUseTimer = false;
float UpdateMappingTimer;
float MappingOverlayPercentage;

ovrTextureSwapChain *MenuSwapChain;
GLuint MenuFrameBuffer = 0;

bool isTransitioning;
int transitionMoveDir = 1;
float transitionState = 1;

void (*updateMappingText)() = nullptr;

uint possibleMappingIndices;

Menu *nextMenu, *currentBottomBar;
Menu romSelectionMenu, mainMenu, settingsMenu, buttonMenuMapMenu, buttonEmulatorMapMenu, bottomBar,
        buttonMappingOverlay, moveMenu;

MenuButton *mappedButton;
MenuButton *backHelp, *menuHelp, *selectHelp;
MenuButton *yawButton, *pitchButton, *rollButton, *scaleButton, *distanceButton;
MenuButton *slotButton;
//MenuButton *menuMappingButtons[];
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

ovrMatrix4f CenterEyeViewMatrix;

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
    OVR_LOG("Pressed RESUME GAME");
    if (loadedRom) menuOpen = false;
}

void OnClickResetGame(MenuItem *item) {
    OVR_LOG("RESET GAME");
    if (loadedRom) {
        Emulator::ResetGame();
        menuOpen = false;
    }
}

void OnClickSaveGame(MenuItem *item) {
    OVR_LOG("on click save game");
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

//    SelectButton = SwappSelectBackButton ? BUTTON_B : BUTTON_A;
//    BackButton = SwappSelectBackButton ? BUTTON_A : BUTTON_B;

    selectHelp->IconId = SwappSelectBackButton ? textureButtonBIconId : textureButtonAIconId;
    backHelp->IconId = SwappSelectBackButton ? textureButtonAIconId : textureButtonBIconId;
}

uint GetPressedButton(uint &_buttonState, uint &_lastButtonState) {
    return _buttonState & ~_lastButtonState;
}

void MoveMenuButtonMapping(MenuItem *item, int dir) {
//    button_mapping_menu_index += dir;
//    if (button_mapping_menu_index > 3) button_mapping_menu_index = 2;
//
//    button_mapping_menu = 0x1 << button_mapping_menu_index;
//    ((MenuButton *) item)->Text = "menu mapped to: " + MapButtonStr[button_mapping_menu_index];
//
//    menuHelp->IconId =
//            button_mapping_menu_index == 2 ? textureButtonXIconId : textureButtonYIconId;
}

void SetMappingText(MenuButton *Button, MappedButton *Mapping) {
    if (Mapping->IsSet)
        Button->SetText(MapButtonStr[Mapping->InputDevice * 32 + Mapping->ButtonIndex]);
    else
        Button->SetText("-");
}

// mapping functions
void UpdateButtonMappingText(MenuItem *item) {
    OVR_LOG("Update mapping text for %i", item->Tag);

    SetMappingText(((MenuButton *) item), &Emulator::buttonMapping[item->Tag].Buttons[item->Tag2]);
}

void UpdateMenuMapping() {
    mappedButton->SetText(MapButtonStr[
                                  buttonMappingMenu.Buttons[mappedButton->Tag].InputDevice * 32 +
                                  buttonMappingMenu.Buttons[mappedButton->Tag].ButtonIndex]);
}

void UpdateEmulatorMapping() {
    Emulator::UpdateButtonMapping();
    SetMappingText(mappedButton, remapButton);
}

void OnMenuMappingButtonSelect(MenuItem *item, int direction) {
    buttonMenuMapMenu.MoveSelection(direction, false);
}

void OnClickMenuMappingButtonLeft(MenuItem *item) {
    buttonMenuMapMenu.CurrentSelection--;
}

void OnClickMenuMappingButtonRight(MenuItem *item) {
    buttonMenuMapMenu.CurrentSelection++;
}

void OnMappingButtonSelect(MenuItem *item, int direction) {
    buttonEmulatorMapMenu.MoveSelection(direction, false);
}

void OnClickMappingButtonLeft(MenuItem *item) {
    buttonEmulatorMapMenu.CurrentSelection--;
}

void OnClickMappingButtonRight(MenuItem *item) {
    buttonEmulatorMapMenu.CurrentSelection++;
}

void OnClickChangeMenuButtonLeft(MenuItem *item) { MoveMenuButtonMapping(item, 1); }

void OnClickChangeMenuButtonRight(MenuItem *item) { MoveMenuButtonMapping(item, 1); }

void OnClickChangeMenuButtonEnter(MenuItem *item) {
    UpdateMapping = true;
    UpdateMappingUseTimer = false;
    remapButton = &buttonMappingMenu.Buttons[item->Tag];
    mappedButton = (MenuButton *) item;
    updateMappingText = UpdateMenuMapping;

    possibleMappingIndices = BUTTON_X | BUTTON_Y;

    mappingButtonLabel->SetText("Menu Button");
    possibleMappingLabel->SetText("(A, B, X, Y,...)");
}

void OnClickChangeButtonMappingEnter(MenuItem *item) {
    UpdateMapping = true;
    UpdateMappingUseTimer = true;
    UpdateMappingTimer = 4.0f;
    // TODO
    remapButton = &Emulator::buttonMapping[item->Tag].Buttons[item->Tag2];
    mappedButton = &buttonMapping.at(item->Tag * 2 + item->Tag2);
    updateMappingText = UpdateEmulatorMapping;

    // buttons from BUTTON_A to BUTTON_RSTICK_RIGHT
    // 4194303;// BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Y;
    possibleMappingIndices = BUTTON_TOUCH - 1;

    // TODO
    mappingButtonLabel->SetText("Button");
    possibleMappingLabel->SetText("(A, B, X, Y,...)");
}

void UpdateButtonMapping(MenuItem *item, uint *_buttonStates, uint *_lastButtonStates) {
    if (!UpdateMapping)
        return;

    for (uint i = 0; i < 3; ++i) {
        uint newButtonState = GetPressedButton(_buttonStates[i], _lastButtonStates[i]);

        if (newButtonState) {
            for (uint j = 0; j < EmuButtonCount; ++j) {
                if (newButtonState & ButtonMapping[j]) {
                    OVR_LOG("mapped to %i", j);
                    UpdateMapping = false;
                    remapButton->InputDevice = i;
                    remapButton->ButtonIndex = j;
                    remapButton->Button = ButtonMapping[j];
                    remapButton->IsSet = true;
                    updateMappingText();
                    break;
                }
            }

            break;
        }
    }

    // ignore button press
    for (int i = 0; i < 3; ++i) {
        _buttonStates[i] = 0;
        _lastButtonStates[i] = 0;
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

float ToDegree(float radian) { return (int) (180.0 / VRAPI_PI * radian * 10) / 10.0F; }

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

    if (LayerBuilder::screenSize < 0.05F) LayerBuilder::screenSize = 0.05F;
    if (LayerBuilder::screenSize > 20.0F) LayerBuilder::screenSize = 20.0F;

    ((MenuButton *) item)->Text = "Scale: " + to_string(LayerBuilder::screenSize);
}

void MoveRoll(MenuItem *item, float dir) {
    LayerBuilder::screenRoll -= dir;
    ((MenuButton *) item)->Text =
            "Roll: " + to_string(ToDegree(LayerBuilder::screenRoll)) + "°";
}

void OnClickResetEmulatorMapping(MenuItem *item) {
    Emulator::RestButtonMapping();

    for (int i = 0; i < Emulator::buttonCount * 2; ++i) {
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
    LayerBuilder::radiusMenuScreen = 0.75F;
    ChangeDistance(distanceButton, 0);
}

void OnClickScale(MenuItem *item) {
    LayerBuilder::screenSize = 1.0F;
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

    OVR_LOG("Set up Menu");
    int bigGap = 10;
    int smallGap = 5;

    OVR_LOG("got emulator");

    menuItemSize = (fontMenu.FontSize + 4);
    strVersionWidth = GetWidth(fontVersion, STR_VERSION);

    {
        OVR_LOG("Set up Rom Selection Menu");
        Emulator::InitRomSelectionMenu(0, 0, romSelectionMenu);

        romSelectionMenu.CurrentSelection = 0;

        romSelectionMenu.BackPress = OnBackPressedRomList;
        romSelectionMenu.Init();
    }

    {
        // TODO: now that the menu button can be mapped to ever button not so sure what to do with this
//        menuHelp = new MenuButton(&fontBottom, buttonMappingMenu.Buttons[0].Button == EmuButton_X ? textureButtonXIconId : textureButtonYIconId,
//                                  "Close Menu", 7, MENU_HEIGHT - BOTTOM_HEIGHT, 0, BOTTOM_HEIGHT, nullptr, nullptr, nullptr);
//        menuHelp->Color = MenuBottomColor;
//        bottomBar.MenuItems.push_back(menuHelp);

        backHelp = new MenuButton(&fontBottom, SwappSelectBackButton ? textureButtonAIconId : textureButtonBIconId,
                                  "Back", MENU_WIDTH - 210, MENU_HEIGHT - BOTTOM_HEIGHT, 0, BOTTOM_HEIGHT, nullptr, nullptr, nullptr);
        backHelp->Color = MenuBottomColor;
        bottomBar.MenuItems.push_back(backHelp);

        selectHelp = new MenuButton(&fontBottom, SwappSelectBackButton ? textureButtonBIconId : textureButtonAIconId,
                                    "Select", MENU_WIDTH - 110, MENU_HEIGHT - BOTTOM_HEIGHT, 0, BOTTOM_HEIGHT, nullptr, nullptr, nullptr);
        selectHelp->Color = MenuBottomColor;
        bottomBar.MenuItems.push_back(selectHelp);

        currentBottomBar = &bottomBar;
    }

    int posX = 20;
    int posY = HEADER_HEIGHT + 20;

    // -- main menu page --
    mainMenu.MenuItems.push_back(new MenuButton(&fontMenu, textureResumeId, "Resume Game", posX, posY, OnClickResumGame, nullptr, nullptr));
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureResetIconId, "Reset Game", posX, posY += menuItemSize, OnClickResetGame, nullptr, nullptr));
    slotButton =
            new MenuButton(&fontMenu, textureSaveSlotIconId, "", posX, posY += menuItemSize + 10, OnClickSaveSlotRight, OnClickSaveSlotLeft,
                           OnClickSaveSlotRight);

    slotButton->ScrollTimeH = 10;
    ChangeSaveSlot(slotButton, 0);
    mainMenu.MenuItems.push_back(slotButton);
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureSaveIconId, "Save", posX, posY += menuItemSize, OnClickSaveGame, nullptr, nullptr));
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureLoadIconId, "Load", posX, posY += menuItemSize, OnClickLoadGame, nullptr, nullptr));
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureLoadRomIconId, "Load Rom", posX, posY += menuItemSize + 10, OnClickLoadRomGame, nullptr, nullptr));
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureResetViewIconId, "Reset View", posX, posY += menuItemSize, OnClickResetView, nullptr, nullptr));
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureSettingsId, "Settings", posX, posY += menuItemSize, OnClickSettingsGame, nullptr, nullptr));
    mainMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureExitIconId, "Exit", posX, posY += menuItemSize + 10, OnClickExit, nullptr, nullptr));

    OVR_LOG("Set up Main Menu");
    Emulator::InitMainMenu(posX, posY, mainMenu);

    mainMenu.Init();
    mainMenu.BackPress = OnClickBackMainMenu;

    // -- settings page --
    posY = HEADER_HEIGHT + 20;

    settingsMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureMappingIconId, "Menu Button Mapping", posX, posY, OnClickMenuMappingScreen, nullptr, nullptr));
    settingsMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureMappingIconId, "Emulator Button Mapping",
                           posX, posY += menuItemSize, OnClickEmulatorMappingScreen, nullptr, nullptr));
    settingsMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureMoveIconId, "Move Screen", posX, posY += menuItemSize, OnClickMoveScreen, nullptr, nullptr));
    settingsMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureFollowHeadIconId, strMove[followHead ? 0 : 1], posX, posY += menuItemSize + bigGap,
                           OnClickFollowMode, OnClickFollowMode, OnClickFollowMode));

    OVR_LOG("Set up Settings Menu");
    Emulator::InitSettingsMenu(posX, posY, settingsMenu);

    settingsMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureBackIconId, "Save and Back",
                           posX, posY += menuItemSize + bigGap, OnClickBackAndSave, nullptr, nullptr));

    settingsMenu.MenuItems.push_back(
            new MenuLabel(&fontVersion, STR_VERSION, MENU_WIDTH - 70, MENU_HEIGHT - BOTTOM_HEIGHT - 50 + 10, 70, 50, textColorVersion));

    settingsMenu.BackPress = OnBackPressedSettings;
    settingsMenu.Init();

    // -- menu button mapping --
    posY = HEADER_HEIGHT + 20;

    MenuButton *swapButton =
            new MenuButton(&fontMenu, textureLoadRomIconId, "", posX, posY, SwapButtonSelectBack, SwapButtonSelectBack, SwapButtonSelectBack);
    swapButton->UpdateFunction = UpdateButtonMapping;
    buttonMenuMapMenu.MenuItems.push_back(swapButton);

    posY += menuItemSize;

    // buttons
    MenuButton *menuMappingButton = new MenuButton(&fontMenu, textureLoadRomIconId, "menu mapped to:", posX, posY, nullptr, nullptr, nullptr);
    menuMappingButton->Selectable = false;
    buttonMenuMapMenu.MenuItems.push_back(menuMappingButton);

    // first button
    MenuButton *menuMappingButton0 = new MenuButton(&fontMenu, 0, MapButtonStr[buttonMappingMenu.Buttons[0].InputDevice * 32 +
                                                                               buttonMappingMenu.Buttons[0].ButtonIndex], 250, posY,
                                                    (MENU_WIDTH - 250 - 20) / 2, menuItemSize, OnClickChangeMenuButtonEnter, nullptr, nullptr);
    menuMappingButton0->Tag = 0;
    menuMappingButton0->RightFunction = OnClickMenuMappingButtonRight;
    buttonMenuMapMenu.MenuItems.push_back(menuMappingButton0);

    // second button
    MenuButton *menuMappingButton1 = new MenuButton(&fontMenu, 0, MapButtonStr[buttonMappingMenu.Buttons[1].InputDevice * 32 +
                                                                               buttonMappingMenu.Buttons[1].ButtonIndex],
                                                    250 + (MENU_WIDTH - 250 - 20) / 2, posY,
                                                    (MENU_WIDTH - 250 - 20) / 2, menuItemSize, OnClickChangeMenuButtonEnter, nullptr, nullptr);
    menuMappingButton1->Tag = 1;
    menuMappingButton1->LeftFunction = OnClickMenuMappingButtonLeft;
    menuMappingButton1->OnSelectFunction = OnMenuMappingButtonSelect;
    buttonMenuMapMenu.MenuItems.push_back(menuMappingButton1);

    SwapButtonSelectBack(swapButton);
    SwapButtonSelectBack(swapButton);

    buttonMenuMapMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureBackIconId, "Back", posX, posY += menuItemSize + bigGap, OnClickBackMove, nullptr, nullptr));
    buttonMenuMapMenu.BackPress = OnBackPressedMove;
    buttonMenuMapMenu.Init();


    // -- emulator button mapping --
    posY = HEADER_HEIGHT + 10;

    for (int i = 0; i < Emulator::buttonCount; ++i) {
        OVR_LOG("Set up mapping for %i", i);

//            MenuLabel *newButtonLabel = new MenuLabel(&fontMenu, "A Button",
//                                                      posX, posY, 150, menuItemSize,
//                                                      {0.9f, 0.9f, 0.9f, 0.9f});

        // image of the button
        auto *newButtonImage =
                new MenuImage(*Emulator::button_icons[i], 80 / 2 - menuItemSize / 2, posY, menuItemSize, menuItemSize, {0.9F, 0.9F, 0.9F, 0.9F});
        buttonEmulatorMapMenu.MenuItems.push_back(newButtonImage);

        int mappingButtonWidth = (MENU_WIDTH - 80 - 20) / 2;
        // left button
        MenuButton *newButtonLeft = new MenuButton(&fontMenu, 0, "abc", 80, posY, mappingButtonWidth, menuItemSize,
                                                   OnClickChangeButtonMappingEnter, nullptr, nullptr);
        newButtonLeft->RightFunction = OnClickMappingButtonRight;
        // @hack: this is only done because the menu currently only really supports lists
        if (i != 0)
            newButtonLeft->OnSelectFunction = OnMappingButtonSelect;
        newButtonLeft->Tag = i;
        newButtonLeft->Tag2 = 0;
        UpdateButtonMappingText(newButtonLeft);
        if (i == 0)
            newButtonLeft->UpdateFunction = UpdateButtonMapping;

        // right button
        MenuButton *newButtonRight =
                new MenuButton(&fontMenu, 0, "abc", 80 + mappingButtonWidth, posY, mappingButtonWidth, menuItemSize,
                               OnClickChangeButtonMappingEnter, nullptr, nullptr);
        newButtonRight->LeftFunction = OnClickMappingButtonLeft;
        // @hack: this is only done because the menu currently only really supports lists
        newButtonRight->OnSelectFunction = OnMappingButtonSelect;
        newButtonRight->Tag = i;
        newButtonRight->Tag2 = 1;
        UpdateButtonMappingText(newButtonRight);

        buttonMapping.push_back(*newButtonLeft);
        buttonMapping.push_back(*newButtonRight);

        posY += menuItemSize;
    }

    MoveMenuButtonMapping(menuMappingButton, 0);

    // select the first element
    buttonEmulatorMapMenu.CurrentSelection = (int) buttonEmulatorMapMenu.MenuItems.size();

    // button mapping page
    for (int i = 0; i < Emulator::buttonCount * 2; ++i)
        buttonEmulatorMapMenu.MenuItems.push_back(&buttonMapping.at(i));

    buttonEmulatorMapMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureResetViewIconId, "Reset Mapping", posX, posY += bigGap, OnClickResetEmulatorMapping, nullptr,
                           nullptr));
    buttonEmulatorMapMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureBackIconId, "Back", posX, posY += menuItemSize - 3, OnClickBackMove, nullptr, nullptr));
    buttonEmulatorMapMenu.BackPress = OnBackPressedMove;
    buttonEmulatorMapMenu.Init();


    // -- button mapping overlay --
    buttonMappingOverlay.MenuItems.push_back(new MenuImage(textureWhiteId, 0, 0, MENU_WIDTH, MENU_HEIGHT, {0.0F, 0.0F, 0.0F, 0.8F}));
    int overlayWidth = 250;
    int overlayHeight = 80;
    int margin = 15;
    buttonMappingOverlay.MenuItems.push_back(
            new MenuImage(textureWhiteId, MENU_WIDTH / 2 - overlayWidth / 2, MENU_HEIGHT / 2 - overlayHeight / 2 - margin, overlayWidth,
                          overlayHeight + margin * 2, {0.05F, 0.05F, 0.05F, 0.3F}));

    mappingButtonLabel = new MenuLabel(&fontMenu, "A Button", MENU_WIDTH / 2 - overlayWidth / 2, MENU_HEIGHT / 2 - overlayHeight / 2,
                                       overlayWidth, overlayHeight / 3, {0.9F, 0.9F, 0.9F, 0.9F});
    possibleMappingLabel = new MenuLabel(&fontMenu, "(A, B, X, Y)", MENU_WIDTH / 2 - overlayWidth / 2,
                                         MENU_HEIGHT / 2 + overlayHeight / 2 - overlayHeight / 3, overlayWidth, overlayHeight / 3,
                                         {0.9F, 0.9F, 0.9F, 0.9F});

    buttonMappingOverlay.MenuItems.push_back(mappingButtonLabel);
    buttonMappingOverlay.MenuItems.push_back(
            new MenuLabel(&fontMenu, "Press Button", MENU_WIDTH / 2 - overlayWidth / 2, MENU_HEIGHT / 2 - overlayHeight / 2 + overlayHeight / 3,
                          overlayWidth, overlayHeight / 3, {0.9F, 0.9F, 0.9F, 0.9F}));
    buttonMappingOverlay.MenuItems.push_back(possibleMappingLabel);

    // -- move menu page --
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
    distanceButton = new MenuButton(&fontMenu, textureDistanceIconId, "", posX, posY += menuItemSize, OnClickDistance, OnClickMoveScreenDistanceLeft,
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

    moveMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureResetViewIconId, "Reset View", posX, posY += menuItemSize + smallGap, OnClickResetViewSettings,
                           nullptr, nullptr));
    moveMenu.MenuItems.push_back(
            new MenuButton(&fontMenu, textureBackIconId, "Back", posX, posY += menuItemSize + bigGap, OnClickBackMove, nullptr, nullptr));

    moveMenu.BackPress = OnBackPressedMove;
    moveMenu.Init();
    // --

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
                    nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat borderColor[] = {0.0F, 0.0F, 0.0F, 0.0F};
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

    OVR_LOG("finished creating screens");
}

void SaveSettings() {
    std::ofstream saveFile(saveFilePath, std::ios::trunc | std::ios::binary);
    saveFile.write(reinterpret_cast<const char *>(&SAVE_FILE_VERSION), sizeof(int));

    Emulator::SaveEmulatorSettings(&saveFile);
    saveFile.write(reinterpret_cast<const char *>(&followHead), sizeof(bool));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenPitch), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenYaw), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenRoll), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::radiusMenuScreen), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&LayerBuilder::screenSize), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[0].InputDevice), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[0].ButtonIndex), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[1].InputDevice), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[1].ButtonIndex), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&SwappSelectBackButton), sizeof(bool));

    saveFile.close();
    OVR_LOG("Saved Settings");
}

void LoadSettings() {
    buttonMappingMenu.Buttons[0].InputDevice = DeviceGamepad;
    buttonMappingMenu.Buttons[0].ButtonIndex = 5;
    // menu button on the left touch controller
    buttonMappingMenu.Buttons[1].InputDevice = DeviceLeftTouch;
    buttonMappingMenu.Buttons[1].ButtonIndex = 12;

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
            loadFile.read((char *) &buttonMappingMenu.Buttons[0].InputDevice, sizeof(int));
            loadFile.read((char *) &buttonMappingMenu.Buttons[0].ButtonIndex, sizeof(int));
            loadFile.read((char *) &buttonMappingMenu.Buttons[1].InputDevice, sizeof(int));
            loadFile.read((char *) &buttonMappingMenu.Buttons[1].ButtonIndex, sizeof(int));
            loadFile.read((char *) &SwappSelectBackButton, sizeof(bool));
        }

        // TODO: reset all loaded settings
        if (loadFile.fail())
            OVR_LOG("Failed Loading Settings");
        else
            OVR_LOG("Settings Loaded");

        loadFile.close();
    }

    buttonMappingMenu.Buttons[0].Button = ButtonMapping[buttonMappingMenu.Buttons[0].ButtonIndex];
    buttonMappingMenu.Buttons[1].Button = ButtonMapping[buttonMappingMenu.Buttons[1].ButtonIndex];

    OVR_LOG("MenuButtons: %d %d", buttonMappingMenu.Buttons[0].Button, buttonMappingMenu.Buttons[0].ButtonIndex);
    OVR_LOG("MenuButtons: %d", buttonMappingMenu.Buttons[1].Button);
}

void ScanDirectory() {
    DIR *dir;
    struct dirent *ent;
    std::string strFullPath;

    if ((dir = opendir(romFolderPath.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            strFullPath = "";
            strFullPath.append(romFolderPath);
            strFullPath.append(ent->d_name);

            if (ent->d_type == 8) {
                std::string strFilename = ent->d_name;

                // check if the filetype is supported by the emulator
                bool supportedFile = false;
                for (const auto &supportedFileName : Emulator::supportedFileNames) {
                    if (strFilename.find(supportedFileName) !=
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
        OVR_LOG("could not open folder");
    }

    OVR_LOG("scanned directory");
}

// void OvrApp::LeavingVrMode() {}

void GetTimeString(std::string &timeString) {
    struct timespec res{};
    clock_gettime(CLOCK_REALTIME, &res);
    time_t t = res.tv_sec;  // just in case types aren't the same
    tm tmv{};
    localtime_r(&t, &tmv);  // populate tmv with local time info

    timeString.clear();
    if (tmv.tm_hour < 10)
        timeString.append("0");
    timeString.append(to_string(tmv.tm_hour));
    timeString.append(":");
    if (tmv.tm_min < 10)
        timeString.append("0");
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

    glClearColor(MenuBackgroundColor.x, MenuBackgroundColor.y, MenuBackgroundColor.z, MenuBackgroundColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw the backgroud image
    //DrawHelper::DrawTexture(textureBackgroundId, 0, 0, menuWidth, menuHeight,
    //                        {0.7f,0.7f,0.7f,1.0f}, 0.985f);

    // header
    DrawHelper::DrawTexture(textureWhiteId, 0, 0, MENU_WIDTH, HEADER_HEIGHT, MenuBackgroundOverlayHeader, 1);
    DrawHelper::DrawTexture(textureWhiteId, 0, MENU_HEIGHT - BOTTOM_HEIGHT, MENU_WIDTH, BOTTOM_HEIGHT, MenuBackgroundOverlayColorLight, 1);

    // icon
    //DrawHelper::DrawTexture(textureHeaderIconId, 0, 0, 75, 75, headerTextColor, 1);

    FontManager::Begin();
    FontManager::RenderText(fontHeader, STR_HEADER, 15, HEADER_HEIGHT / 2 - fontHeader.PHeight / 2 - fontHeader.PStart, 1.0F, headerTextColor, 1);

    // update the battery string
    int batteryWidth = 10;
    int maxHeight = fontBattery.PHeight + 1;
    int distX = 15;
    int distY = 2;
    int batteryPosY = HEADER_HEIGHT / 2 + distY + 3;

    // update the time string
    GetTimeString(time_string);
    FontManager::RenderText(fontTime, time_string, MENU_WIDTH - time_string_width - distX,
                            HEADER_HEIGHT / 2 - distY - fontBattery.FontSize +
                            fontBattery.PStart, 1.0F, textColorBattery, 1);

    GetBattryString(battery_string);
    FontManager::RenderText(fontBattery, battery_string, MENU_WIDTH - batter_string_width - batteryWidth - 7 - distX,
                            HEADER_HEIGHT / 2 + distY + 3, 1.0F, textColorBattery, 1);

    // FontManager::RenderText(fontSmall, STR_VERSION, menuWidth - strVersionWidth - 7.0f,
    //                        HEADER_HEIGHT - 21, 1.0f, textColorVersion, 1);
    FontManager::Close();

    // draw battery
    DrawHelper::DrawTexture(textureWhiteId, MENU_WIDTH - batteryWidth - distX - 2 - 2, batteryPosY,
                            batteryWidth + 4, maxHeight + 4, BatteryBackgroundColor, 1);

    // calculate the battery color
    float colorState = ((batteryLevel * 10) % (1000 / (batteryColorCount))) / (float) (1000 / batteryColorCount);
    int currentColor = (int) (batteryLevel / (100.0F / batteryColorCount));
    ovrVector4f batteryColor = ovrVector4f{
            BatteryColors[currentColor].x * (1 - colorState) +
            BatteryColors[currentColor + 1].x * colorState,
            BatteryColors[currentColor].y * (1 - colorState)
            + BatteryColors[currentColor + 1].y * colorState,
            BatteryColors[currentColor].z * (1 - colorState)
            + BatteryColors[currentColor + 1].z * colorState,
            BatteryColors[currentColor].w * (1 - colorState)
            + BatteryColors[currentColor + 1].w * colorState
    };

    int height = (int) (batteryLevel / 100.0F * maxHeight);

    DrawHelper::DrawTexture(textureWhiteId, MENU_WIDTH - batteryWidth - distX - 2, batteryPosY + 2 + maxHeight - height,
                            batteryWidth, height, batteryColor, 1);

    DrawMenu();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static inline ovrLayerProjection2 vbLayerProjection() {
    ovrLayerProjection2 layer = {};

    const ovrMatrix4f
            projectionMatrix = ovrMatrix4f_CreateProjectionFov(90.0F, 90.0F, 0.0F, 0.0F, 0.1F,
                                                               0.0F);
    const ovrMatrix4f
            texCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(
            &projectionMatrix);

    layer.Header.Type = VRAPI_LAYER_TYPE_PROJECTION2;
    layer.Header.Flags = 0;
    layer.Header.ColorScale.x = 1.0F;
    layer.Header.ColorScale.y = 1.0F;
    layer.Header.ColorScale.z = 1.0F;
    layer.Header.ColorScale.w = 1.0F;
    layer.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_ONE;
    layer.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ZERO;
    //layer.Header.SurfaceTextureObject = NULL;

    layer.HeadPose.Pose.Orientation.w = 1.0F;

    for (auto &Texture : layer.Textures) {
        Texture.TexCoordsFromTanAngles = texCoordsFromTanAngles;
        Texture.TextureRect.x = -1.0F;
        Texture.TextureRect.y = -1.0F;
        Texture.TextureRect.width = 2.0F;
        Texture.TextureRect.height = 2.0F;
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
        currentMenu->Update(buttonStates, lastButtonStates);
    }
}

ovrFrameResult MenuGo::Update(App *app, const ovrFrameInput &vrFrame) {
    // time:
    // vrFrame.PredictedDisplayTimeInSeconds

    for (int i = 0; i < 3; ++i)
        lastButtonStates[i] = buttonStatesReal[i];

    // UpdateInput(app);
    UpdateInputDevices(app, vrFrame);

    // update button mapping timer
    if (UpdateMapping && UpdateMappingUseTimer) {
        UpdateMappingTimer -= vrFrame.DeltaSeconds;
        mappingButtonLabel->SetText(to_string((int) UpdateMappingTimer));

        if ((int) UpdateMappingTimer <= 0) {
            UpdateMapping = false;
            remapButton->IsSet = false;
            updateMappingText();
        }
    }

    // TODO speed
    if (!menuOpen) {
        if (transitionPercentage > 0) transitionPercentage -= OPEN_MENU_SPEED;
        if (transitionPercentage < 0) transitionPercentage = 0;

        Emulator::Update(vrFrame, buttonStates, lastButtonStates);
    } else {
        if (transitionPercentage < 1) transitionPercentage += OPEN_MENU_SPEED;
        if (transitionPercentage > 1) transitionPercentage = 1;

        UpdateCurrentMenu();
    }

    // open/close menu
    if (loadedRom &&
        ((buttonStates[buttonMappingMenu.Buttons[0].InputDevice] & buttonMappingMenu.Buttons[0].Button &&
          !(lastButtonStates[buttonMappingMenu.Buttons[0].InputDevice] & buttonMappingMenu.Buttons[0].Button)) ||
         (buttonStates[buttonMappingMenu.Buttons[1].InputDevice] & buttonMappingMenu.Buttons[1].Button &&
          !(lastButtonStates[buttonMappingMenu.Buttons[1].InputDevice] & buttonMappingMenu.Buttons[1].Button)))) {
        menuOpen = !menuOpen;
    }

    CenterEyeViewMatrix = vrapi_GetViewMatrixFromPose(&vrFrame.Tracking.HeadPose.Pose);

    //res.Surfaces.PushBack( ovrDrawSurface( &SurfaceDef ) );

    // Clear the eye buffers to 0 alpha so the overlay plane shows through.
    ovrFrameResult res;
    res.ClearColorBuffer = true;
    res.ClearColor = Vector4f(0.0F, 0.0F, 0.0F, 1.0F);
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
        res.FrameMatrices.EyeProjection[eye] = ovrMatrix4f_CreateProjectionFov(vrFrame.FovX, vrFrame.FovY, 0.0F, 0.0F, 1.0F, 0.0F);

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
                MenuSwapChain, MENU_WIDTH, MENU_HEIGHT, &vrFrame.Tracking, followHead, transitionP);

        res.Layers[res.LayerCount].Cylinder.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
        res.Layers[res.LayerCount].Cylinder.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

        res.Layers[res.LayerCount].Cylinder.Header.ColorScale = {transitionP, transitionP, transitionP, transitionP};
        res.Layers[res.LayerCount].Cylinder.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
        res.Layers[res.LayerCount].Cylinder.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

        res.LayerCount++;
    }

    if (resetView) {
        app->RecenterYaw(false);
        resetView = false;
    }
    if (showExitDialog) {
        app->ShowSystemUI(VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
        showExitDialog = false;
    }

    return res;
}

void MenuGo::DrawMenu() {
    // the
    float trProgressOut = ((transitionState - 0.35F) / 0.65F);
    float progressOut = sinf(trProgressOut * MATH_FLOAT_PIOVER2);
    if (trProgressOut < 0)
        trProgressOut = 0;

    float trProgressIn = (((1 - transitionState) - 0.35F) / 0.65F);
    float progressIn = sinf(trProgressIn * MATH_FLOAT_PIOVER2);
    if (transitionState < 0)
        trProgressIn = 0;

    int dist = 75;

    if (UpdateMapping) {
        MappingOverlayPercentage += 0.2F;
        if (MappingOverlayPercentage > 1)
            MappingOverlayPercentage = 1;
    } else {
        MappingOverlayPercentage -= 0.2F;
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

    if (MappingOverlayPercentage > 0) {
        buttonMappingOverlay.Draw(0, -1, (1 - sinf(MappingOverlayPercentage * MATH_FLOAT_PIOVER2)), dist, MappingOverlayPercentage);
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


//---------------------------------------------------------------------------------------------------
// Input device management
//---------------------------------------------------------------------------------------------------

std::vector<ovrInputDeviceBase *> InputDevices;
double LastGamepadUpdateTimeInSeconds;

//==============================
// ovrVrController::FindInputDevice
int FindInputDevice(const ovrDeviceID deviceID) {
    for (int i = 0; i < (int) InputDevices.size(); ++i) {
        if (InputDevices[i]->GetDeviceID() == deviceID) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrVrController::RemoveDevice
void RemoveDevice(const ovrDeviceID deviceID) {
    int index = FindInputDevice(deviceID);
    if (index < 0) {
        return;
    }
    ovrInputDeviceBase *device = InputDevices[index];
    delete device;
    InputDevices[index] = InputDevices.back();
    InputDevices[InputDevices.size() - 1] = nullptr;
    InputDevices.pop_back();
}

//==============================
// ovrVrController::IsDeviceTracked
bool IsDeviceTracked(const ovrDeviceID deviceID) {
    return FindInputDevice(deviceID) >= 0;
}

//==============================
// ovrVrController::OnDeviceConnected
void OnDeviceConnected(App *app, const ovrInputCapabilityHeader &capsHeader) {
    ovrInputDeviceBase *device = nullptr;
    ovrResult result = ovrError_NotInitialized;

    switch (capsHeader.Type) {
        case ovrControllerType_Gamepad: {
            OVR_LOG_WITH_TAG("MLBUConnect", "Gamepad connected, ID = %u", capsHeader.DeviceID);

            ovrInputGamepadCapabilities gamepadCapabilities;
            gamepadCapabilities.Header = capsHeader;
            result = vrapi_GetInputDeviceCapabilities(app->GetOvrMobile(),
                                                      &gamepadCapabilities.Header);

            if (result == ovrSuccess)
                device = ovrInputDevice_Gamepad::Create(*app, gamepadCapabilities);

            break;
        }

        case ovrControllerType_TrackedRemote: {
            OVR_LOG_WITH_TAG("MLBUConnect", "Controller connected, ID = %u", capsHeader.DeviceID);

            ovrInputTrackedRemoteCapabilities remoteCapabilities;
            remoteCapabilities.Header = capsHeader;

            result = vrapi_GetInputDeviceCapabilities(app->GetOvrMobile(),
                                                      &remoteCapabilities.Header);

            if (result == ovrSuccess) {
                device = ovrInputDevice_TrackedRemote::Create(*app, remoteCapabilities);
            }
            break;
        }

        default:
            OVR_LOG("Unknown device connected!");
            OVR_ASSERT(false);
            return;
    }

    if (result != ovrSuccess) {
        OVR_LOG_WITH_TAG("MLBUConnect", "vrapi_GetInputDeviceCapabilities: Error %i", result);
    }

    if (device != nullptr) {
        OVR_LOG_WITH_TAG("MLBUConnect", "Added device '%s', id = %u", device->GetName(),
                         capsHeader.DeviceID);
        InputDevices.push_back(device);
    } else {
        OVR_LOG_WITH_TAG("MLBUConnect", "Device creation failed for id = %u", capsHeader.DeviceID);
    }
}

//==============================
// ovrVrController::EnumerateInputDevices
void EnumerateInputDevices(App *app) {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(app->GetOvrMobile(), deviceIndex, &curCaps) < 0) {
            //OVR_LOG_WITH_TAG( "Input", "No more devices!" );
            break;    // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID)) {
            OVR_LOG_WITH_TAG("Input", "     tracked");
            OnDeviceConnected(app, curCaps);
        }
    }
}

//==============================
// ovrVrController::OnDeviceDisconnected
void OnDeviceDisconnected(const ovrDeviceID deviceID) {
    OVR_LOG_WITH_TAG("MLBUConnect", "Controller disconnected, ID = %i", deviceID);
    RemoveDevice(deviceID);
}

//==============================
// ovrInputDevice_Gamepad::Create
ovrInputDeviceBase *
ovrInputDevice_Gamepad::Create(App &app, const ovrInputGamepadCapabilities &gamepadCapabilities) {
    OVR_LOG_WITH_TAG("MLBUConnect", "Gamepad");

    auto *device = new ovrInputDevice_Gamepad(gamepadCapabilities);
    return device;
}

//==============================
// ovrInputDevice_TrackedRemote::Create
ovrInputDeviceBase *ovrInputDevice_TrackedRemote::Create(App &app,
                                                         const ovrInputTrackedRemoteCapabilities &remoteCapabilities) {
    OVR_LOG_WITH_TAG("MLBUConnect", "ovrInputDevice_TrackedRemote::Create");

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = remoteCapabilities.Header.Type;
    ovrResult result = vrapi_GetCurrentInputState(app.GetOvrMobile(),
                                                  remoteCapabilities.Header.DeviceID,
                                                  &remoteInputState.Header);

    if (result == ovrSuccess) {
        auto *device = new ovrInputDevice_TrackedRemote(remoteCapabilities,
                                                        remoteInputState.RecenterCount);

        return device;
    } else {
        OVR_LOG_WITH_TAG("MLBUConnect", "vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}

ovrResult PopulateRemoteControllerInfo(App *app, ovrInputDevice_TrackedRemote &trDevice) {
    ovrDeviceID deviceID = trDevice.GetDeviceID();

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = trDevice.GetType();

    ovrResult result;
    result = vrapi_GetCurrentInputState(app->GetOvrMobile(), deviceID, &remoteInputState.Header);

    if (result != ovrSuccess) {
        OVR_LOG_WITH_TAG("MLBUState", "ERROR %i getting remote input state!", result);
        OnDeviceDisconnected(deviceID);
        return result;
    }

    const auto *inputTrackedRemoteCapabilities = reinterpret_cast<const ovrInputTrackedRemoteCapabilities *>( trDevice.GetCaps());

    if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_ModelOculusTouch) {
        if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_LeftHand) {

            buttonStatesReal[1] = remoteInputState.Buttons;

            // the directions seem to be mirrored on the touch controllers compared to the gamepad
            buttonStatesReal[1] |= (remoteInputState.Joystick.x < -0.5f) ? EmuButton_Left : 0;
            buttonStatesReal[1] |= (remoteInputState.Joystick.x > 0.5f) ? EmuButton_Right : 0;
            buttonStatesReal[1] |= (remoteInputState.Joystick.y < -0.5f) ? EmuButton_Down : 0;
            buttonStatesReal[1] |= (remoteInputState.Joystick.y > 0.5f) ? EmuButton_Up : 0;

            buttonStatesReal[1] |= (remoteInputState.IndexTrigger > 0.25f) ? EmuButton_Trigger : 0;
            buttonStatesReal[1] |= (remoteInputState.GripTrigger > 0.25f) ? EmuButton_GripTrigger : 0;

        } else if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_RightHand) {

            buttonStatesReal[2] = remoteInputState.Buttons;

            buttonStatesReal[2] |= (remoteInputState.Joystick.x < -0.5f) ? EmuButton_Left : 0;
            buttonStatesReal[2] |= (remoteInputState.Joystick.x > 0.5f) ? EmuButton_Right : 0;
            buttonStatesReal[2] |= (remoteInputState.Joystick.y < -0.5f) ? EmuButton_Down : 0;
            buttonStatesReal[2] |= (remoteInputState.Joystick.y > 0.5f) ? EmuButton_Up : 0;

            buttonStatesReal[2] |= (remoteInputState.IndexTrigger > 0.25f) ? EmuButton_Trigger : 0;
            buttonStatesReal[2] |= (remoteInputState.GripTrigger > 0.25f) ? EmuButton_GripTrigger : 0;
        }
    }

    if (remoteInputState.RecenterCount != trDevice.GetLastRecenterCount()) {
        OVR_LOG_WITH_TAG("MLBUState", "**RECENTERED** (%i != %i )", (int) remoteInputState.RecenterCount, (int) trDevice.GetLastRecenterCount());
        trDevice.SetLastRecenterCount(remoteInputState.RecenterCount);
    }

    return result;
}

void UpdateInputDevices(App *app, const ovrFrameInput &vrFrame) {

    for (int i = 0; i < 3; ++i) {
        buttonStatesReal[i] = 0;
        buttonStates[i] = 0;
    }

    EnumerateInputDevices(app);

    // for each device, query its current tracking state and input state
    // it's possible for a device to be removed during this loop, so we go through it backwards
    for (int i = (int) InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase *device = InputDevices[i];
        if (device == nullptr) {
            OVR_ASSERT(false);    // this should never happen!
            continue;
        }

        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            OVR_ASSERT(deviceID != ovrDeviceIdType_Invalid);
            continue;
        } else if (device->GetType() == ovrControllerType_Gamepad) {

            if (deviceID != ovrDeviceIdType_Invalid) {
                ovrInputStateGamepad gamepadInputState;
                gamepadInputState.Header.ControllerType = ovrControllerType_Gamepad;
                ovrResult result = vrapi_GetCurrentInputState(app->GetOvrMobile(), deviceID,
                                                              &gamepadInputState.Header);

                if (result == ovrSuccess &&
                    gamepadInputState.Header.TimeInSeconds >= LastGamepadUpdateTimeInSeconds) {
                    LastGamepadUpdateTimeInSeconds = gamepadInputState.Header.TimeInSeconds;

                    // not so sure if this is such a good idea
                    // if they change the order of the buttons this will break
                    buttonStatesReal[0] = gamepadInputState.Buttons;

                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.x < -0.5f) ? EmuButton_LeftStickLeft : 0;
                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.x > 0.5f) ? EmuButton_LeftStickRight : 0;
                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.y < -0.5f) ? EmuButton_LeftStickUp : 0;
                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.y > 0.5f) ? EmuButton_LeftStickDown : 0;

                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.x < -0.5f) ? EmuButton_RightStickLeft : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.x > 0.5f) ? EmuButton_RightStickRight : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.y < -0.5f) ? EmuButton_RightStickUp : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.y > 0.5f) ? EmuButton_RightStickDown : 0;

                    buttonStatesReal[0] |= (gamepadInputState.LeftTrigger > 0.25f) ? EmuButton_L2 : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightTrigger > 0.25f) ? EmuButton_R2 : 0;
                }
            }
        } else if (device->GetType() == ovrControllerType_TrackedRemote) {
            if (deviceID != ovrDeviceIdType_Invalid) {
                ovrInputDevice_TrackedRemote &trDevice = *dynamic_cast< ovrInputDevice_TrackedRemote *>( device );

                ovrTracking remoteTracking;
                ovrResult result = vrapi_GetInputTrackingState(
                        app->GetOvrMobile(), deviceID, vrFrame.PredictedDisplayTimeInSeconds, &remoteTracking);
                if (result != ovrSuccess) {
                    OnDeviceDisconnected(deviceID);
                    continue;
                }

                trDevice.SetTracking(remoteTracking);

                PopulateRemoteControllerInfo(app, trDevice);
            }
        } else {
            OVR_LOG_WITH_TAG("MLBUState", "Unexpected Device Type %d on %d", device->GetType(), i);
        }

        buttonStates[0] = buttonStatesReal[0];
        buttonStates[1] = buttonStatesReal[1];
        buttonStates[2] = buttonStatesReal[2];
    }
}