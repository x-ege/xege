#include "ege.h"
#include "ege/button.h"
#include "ege/egecontrolbase.h"
#include "ege/fps.h"
#include "ege/label.h"
#include "ege/sys_edit.h"
#include "../test_opengl_mode.h"
#include "../test_shutdown.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

int failures = 0;

void stage(const char* message)
{
    std::cerr << "control_contract: " << message << std::endl;
}

void expect(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

int changedPixels(ege::PCIMAGE image, ege::color_t background)
{
    int changed = 0;
    for (int y = 0; y < ege::getheight(image); ++y) {
        for (int x = 0; x < ege::getwidth(image); ++x) {
            if (ege::getpixel(x, y, image) != background) {
                ++changed;
            }
        }
    }
    return changed;
}

class ProbeControl : public ege::egeControlBase
{
public:
    CTL_PREINIT(ProbeControl, egeControlBase) {}
    CTL_PREINITEND;

    ProbeControl(CTL_DEFPARAM) : CTL_INITBASE(egeControlBase)
    {
        CTL_INIT;
        keyDown = keyUp = keyChar = mouseCalls = 0;
        consumeKeyUp = consumeKeyChar = false;
        size(24, 16);
    }

    int onKeyDown(int, int)
    {
        ++keyDown;
        return 0;
    }

    int onKeyUp(int, int)
    {
        ++keyUp;
        return consumeKeyUp ? 1 : 0;
    }

    int onKeyChar(int, int)
    {
        ++keyChar;
        return consumeKeyChar ? 1 : 0;
    }

    int onMouse(int, int, int)
    {
        ++mouseCalls;
        return 0;
    }

    int keyDown;
    int keyUp;
    int keyChar;
    int mouseCalls;
    bool consumeKeyUp;
    bool consumeKeyChar;
};

class CountingButton : public ege::button
{
public:
    CountingButton(int inherit = inherit_level_e,
                   ege::egeControlBase* parent = NULL)
        : button(inherit, parent), clicks(0)
    {
    }

    void onClick() { ++clicks; }
    int clicks;
};

} // namespace

int main()
{
    const ege::initmode_flag mode = with_opengl_test_mode(
        static_cast<ege::initmode_flag>(
            ege::INIT_RENDERMANUAL | ege::INIT_NOFORCEEXIT | ege::INIT_HIDE));
    ege::initgraph(128, 96, mode);
    if (!ege::getHWnd()) {
        std::cerr << "FAIL: unable to create the hidden control test context\n";
        return EXIT_FAILURE;
    }
    stage("graphics initialized");

    ProbeControl* detachedChild = NULL;
    {
        ProbeControl* root = new ProbeControl;
        ege::egeControlBase* survivingParent = root->parent();
        detachedChild = new ProbeControl(ProbeControl::inherit_level_e, root);
        expect(detachedChild->parent() == root,
               "a child records its explicit parent");
        delete root;
        expect(detachedChild->parent() == survivingParent,
               "destroying a control promotes surviving children to its parent");
    }
    delete detachedChild;
    stage("detached-child promotion completed");

    {
        stage("main control tree started");
        ProbeControl root;
        ProbeControl parent(ProbeControl::inherit_level_e, &root);
        ProbeControl child(ProbeControl::inherit_level_e, &parent);
        stage("main controls constructed");
        expect(child.parent() == &parent, "nested controls keep their parent");
        parent.move(7, 9);
        expect(parent.getx() == 7 && parent.gety() == 9,
               "move updates control coordinates");
        parent.size(30, 20);
        expect(parent.width() == 30 && parent.height() == 20 &&
                   ege::getwidth(parent.buf()) == 30 &&
                   ege::getheight(parent.filter()) == 20,
               "size updates dimensions and both backing images");

        parent.visible(false);
        parent.enable(false);
        parent.autoredraw(false);
        parent.directdraw(true);
        parent.capture(true);
        parent.capmouse(true);
        expect(!parent.isvisible() && !parent.isenable() &&
                   !parent.isautoredraw() && parent.isdirectdraw() &&
                   parent.iscapture() && parent.iscapmouse(),
               "control state setters round-trip through their getters");
        parent.visible(true);
        parent.enable(true);
        child.capture(true);

        parent.keymsgdown('A', 1);
        expect(parent.keyDown == 1 && child.keyDown == 1,
               "unconsumed key-down events reach captured children");
        parent.consumeKeyUp = true;
        parent.keymsgup('A', 2);
        expect(parent.keyUp == 1 && child.keyUp == 0,
               "consumed key-up events do not reach children");
        parent.consumeKeyUp = false;
        parent.keymsgup('A', 2);
        expect(child.keyUp == 1,
               "unconsumed key-up events reach captured children");
        parent.consumeKeyChar = true;
        parent.keymsgchar('x', 3);
        expect(parent.keyChar == 1 && child.keyChar == 0,
               "consumed character events do not reach children");
        parent.consumeKeyChar = false;
        parent.keymsgchar('y', 3);
        expect(child.keyChar == 1,
               "unconsumed character events reach captured children");
        stage("base control checks completed");

        {
            ProbeControl promoted(ProbeControl::inherit_level_e, &parent);
            expect(promoted.parent() == &parent,
                   "a second nested child starts under the intermediate parent");
        }

        ege::label text(ege::label::inherit_level_e, &root);
        expect(std::strcmp(text.caption(), "") == 0,
               "label initializes an empty caption");
        text.caption("control");
        text.font("SimSun");
        text.fontsize(14);
        text.color(ege::YELLOW);
        text.bkcolor(ege::BLUE);
        text.transparent(true);
        text.alpha(300);
        expect(std::strcmp(text.caption(), "control") == 0 &&
                   std::strcmp(text.font(), "SimSun") == 0 &&
                   text.fontsize() == 14 && text.color() == ege::YELLOW &&
                   text.bkcolor() == ege::BLUE && text.transparent() &&
                   text.alpha() == 255,
               "label properties and alpha clamping round-trip");
        stage("label properties completed");

        CountingButton action(CountingButton::inherit_level_e, &root);
        action.caption("run");
        action.font("SimSun");
        action.fontsize(13);
        action.alpha(-1);
        action.bgcolor(ege::BLUE);
        action.linecolor(ege::WHITE);
        action.shadowcolor(ege::BLACK);
        action.textcolor(ege::YELLOW);
        expect(std::strcmp(action.caption(), "run") == 0 &&
                   std::strcmp(action.font(), "SimSun") == 0 &&
                   action.fontsize() == 13 && action.alpha() == 0 &&
                   action.bgcolor() == ege::BLUE &&
                   action.linecolor() == ege::WHITE &&
                   action.shadowcolor() == ege::BLACK &&
                   action.textcolor() == ege::YELLOW,
               "button properties and alpha clamping round-trip");
        action.onKeyDown(13, 0);
        action.onKeyUp(13, 0);
        action.onMouse(1, 1, ege::mouse_msg_down | ege::mouse_flag_left);
        action.onMouse(1, 1, ege::mouse_msg_up | ege::mouse_flag_left);
        expect(action.clicks == 2,
               "button dispatches keyboard and mouse clicks");
        action.onLostFocus();
        stage("button checks completed");

        ege::PIMAGE target = ege::newimage(96, 64);
        ege::setbkcolor(ege::BLACK, target);
        ege::cleardevice(target);
        text.directdraw(false);
        text.transparent(false);
        text.alpha(255);
        text.draw(target);
        expect(changedPixels(target, ege::BLACK) > 0,
               "label draw contributes visible pixels");
        stage("label drawing completed");

        ege::fps counter(ege::fps::inherit_level_e, &root);
        expect(counter.isdirectdraw() && !counter.isenable(),
               "fps keeps its documented direct-draw, disabled defaults");
        ege::setbkcolor(ege::BLACK, target);
        ege::cleardevice(target);
        counter.onDraw(target);
        expect(changedPixels(target, ege::BLACK) > 0,
               "fps draws its current value");
        stage("fps drawing completed");

        ege::PIMAGE previousTarget = ege::gettarget();
        {
            ege::PushTarget pushed(target);
            expect(ege::gettarget() == target,
                   "PushTarget selects its requested image");
        }
        expect(ege::gettarget() == previousTarget,
               "PushTarget restores the previous image");
        ege::delimage(target);
        stage("drawing checks completed");

        ege::sys_edit edit(ege::sys_edit::inherit_level_e, &root);
#ifdef _WIN32
        stage("creating sys_edit");
        expect(edit.create() == ege::grOk,
               "sys_edit creates a native Win32 control");
        stage("destroying sys_edit");
        expect(edit.destroy() == 1,
               "sys_edit destroys its native Win32 control");
        stage("sys_edit checks completed");
#else
        expect(edit.create() == ege::grError,
               "sys_edit reports that no native Unix edit control exists");
        expect(edit.destroy() == 0,
               "destroy is harmless when no native edit control exists");
#endif
    }
    stage("main control tree destroyed");

    expect(shutdown_graphics_for_test(),
           "the control test window and UI thread shut down cleanly");
    stage("graphics shutdown completed");

    if (failures != 0) {
        std::cerr << failures << " control contract assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All control contract assertions passed\n";
    return EXIT_SUCCESS;
}
