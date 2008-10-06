#include "IrrlichtBase.h"
#include "ncbind/ncbind.hpp"

extern void message_log(const char* format, ...);
extern void error_log(const char *format, ...);

using namespace irr;
using namespace core;
using namespace video;
using namespace scene;
using namespace io;
using namespace gui;

// -----------------------------------
// 各種オブジェクト参照用
// -----------------------------------

bool IsArray(tTJSVariant &var)
{
	if (var.Type() == tvtObject) {
		iTJSDispatch2 *obj = var.AsObjectNoAddRef();
		return obj->IsInstanceOf(0, NULL, NULL, L"Array", obj) == TJS_S_TRUE;
	}
	return false;
}

dimension2df getDimension2D(tTJSVariant &var)
{
	ncbPropAccessor info(var);
	if (IsArray(var)) {
		return dimension2df((f32)info.getRealValue(0),
							(f32)info.getRealValue(1));
	} else {
		return dimension2df((f32)info.getRealValue(L"width"),
							(f32)info.getRealValue(L"height"));
	}
}

position2df getPosition2D(tTJSVariant &var)
{
	ncbPropAccessor info(var);
	if (IsArray(var)) {
		return position2df((f32)info.getRealValue(0),
						   (f32)info.getRealValue(1));
	} else {
		return position2df((f32)info.getRealValue(L"x"),
						   (f32)info.getRealValue(L"y"));
	}
}

vector2df getVector2D(tTJSVariant &var)
{
	ncbPropAccessor info(var);
	if (IsArray(var)) {
		return vector2df((f32)info.getRealValue(0),
						 (f32)info.getRealValue(1));
	} else {
		return vector2df((f32)info.getRealValue(L"x"),
						 (f32)info.getRealValue(L"y"));
	}
}

vector3df getVector3D(tTJSVariant &var)
{
	ncbPropAccessor info(var);
	if (IsArray(var)) {
		return vector3df((f32)info.getRealValue(0),
						 (f32)info.getRealValue(1),
						 (f32)info.getRealValue(2));
	} else {
		return vector3df((f32)info.getRealValue(L"x"),
						 (f32)info.getRealValue(L"y"),
						 (f32)info.getRealValue(L"z"));
	}
}

rect<s32> getRect(tTJSVariant &var)
{
	ncbPropAccessor info(var);
	if (IsArray(var)) {
		return rect<s32>(info.getIntValue(0),
						 info.getIntValue(1),
						 info.getIntValue(2),
						 info.getIntValue(3));
	} else {
		int x = info.getIntValue(L"x");
		int y = info.getIntValue(L"y");
		int x2, y2;
		if (info.HasValue(L"width")) {
			x2 = x + info.getIntValue(L"width") - 1;
			y2 = x + info.getIntValue(L"height") - 1;
		} else {
			x2 = info.getIntValue(L"x2");
			y2 = info.getIntValue(L"y2");
		}
		return rect<s32>(x, y, x2, y2);
	}
}

// ----------------------------------- 

/**
 * コンストラクタ
 */
IrrlichtBase::IrrlichtBase(video::E_DRIVER_TYPE driverType) : driverType(driverType), device(NULL)
{
}

/**
 * デストラクタ
 */
IrrlichtBase::~IrrlichtBase()
{
	detach();
}

/**
 * ドライバの情報表示
 */
void
IrrlichtBase::showDriverInfo()
{
	IVideoDriver *driver = device->getVideoDriver();
	if (driver) {
		dimension2d<s32> size = driver->getScreenSize();
		message_log("デバイス生成後のスクリーンサイズ:%d, %d", size.Width, size.Height);
		size = driver->getCurrentRenderTargetSize();
		message_log("デバイス生成後のRenderTargetの:%d, %d", size.Width, size.Height);
	}
}

/**
 * ウインドウの再設定
 * @param hwnd ハンドル
 */
void
IrrlichtBase::attach(HWND hwnd)
{
	// デバイス生成
	SIrrlichtCreationParameters params;
	params.WindowId     = reinterpret_cast<void*>(hwnd);
	params.DriverType    = driverType;
	params.Bits          = 32;
	params.Stencilbuffer = true;
	params.Vsync = true;
	params.EventReceiver = this;
	params.AntiAlias = true;

	if ((device = irr::createDeviceEx(params))) {
		TVPAddLog(L"Irrlichtデバイス初期化");
	} else {
		TVPThrowExceptionMessage(L"Irrlicht デバイスの初期化に失敗しました");
	}
	
	showDriverInfo();
	start();
}

/**
 * ウインドウの解除
 */
void
IrrlichtBase::detach()
{
	if (device) {
		device->drop();
		device = NULL;
	}
	stop();
}

/**
 * Irrlicht 呼び出し処理開始
 */
void
IrrlichtBase::start()
{
	stop();
	TVPAddContinuousEventHook(this);
}

/**
 * Irrlicht 呼び出し処理停止
 */
void
IrrlichtBase::stop()
{
	TVPRemoveContinuousEventHook(this);
}

/**
 * Continuous コールバック
 * 吉里吉里が暇なときに常に呼ばれる
 * これが事実上のメインループになる
 */
void TJS_INTF_METHOD
IrrlichtBase::OnContinuousCallback(tjs_uint64 tick)
{
	if (device) {
		// 時間を進める XXX tick を外部から与えられないか？
		device->getTimer()->tick();

		IVideoDriver *driver = device->getVideoDriver();
		// 描画開始
		if (driver && driver->beginScene(true, true, irr::video::SColor(255,0,0,0))) {

			/// シーンマネージャの描画
			ISceneManager *smgr = device->getSceneManager();
			if (smgr) {
				smgr->drawAll();
			}

			// 固有処理
			update(driver, tick);

			// GUIの描画
			IGUIEnvironment *gui = device->getGUIEnvironment();
			if (gui) {
				gui->drawAll();
			}

			// 描画完了
			driver->endScene();
		}
	}
};

/**
 * イベント受理
 * HWND を指定して生成している関係で Irrlicht 自身はウインドウから
 * イベントを取得することはない。ので GUI Environment からのイベント
 * だけがここにくることになる？
 * @param event イベント情報
 * @return 処理したら true
 */
bool
IrrlichtBase::OnEvent(const irr::SEvent &event)
{
	switch (event.EventType) {
	case EET_GUI_EVENT:
		message_log("GUIイベント:%d", event.GUIEvent.EventType);
		switch(event.GUIEvent.EventType) {
		case EGET_BUTTON_CLICKED:
			message_log("ボタンおされた");
			break;
		}
		break;
	case EET_MOUSE_INPUT_EVENT:
#if 0
		message_log("マウスイベント:%d x:%d y:%d wheel:%f",
			event.MouseInput.Event,
			event.MouseInput.X,
			event.MouseInput.Y,
			event.MouseInput.Wheel);
#endif
		break;
	case EET_KEY_INPUT_EVENT:
//		message_log("キーイベント:%x", event.KeyInput.Key);
		{
			int shift = 0;
			if (event.KeyInput.Shift) {
				shift |= 0x01;
			}
			if (event.KeyInput.Control) {
				shift |= 0x04;
			}
		}
		break;
	case EET_LOG_TEXT_EVENT:
		message_log("ログレベル:%d ログ:%s",
					event.LogEvent.Level,
					event.LogEvent.Text);
		break;
	case EET_USER_EVENT:
		message_log("ユーザイベント");
		break;
	}
	return false;
}


/**
 * @return ドライバ情報の取得
 */
IrrlichtDriver
IrrlichtBase::getDriver()
{
	return device ? IrrlichtDriver(device->getVideoDriver()) : IrrlichtDriver();
}
	
/**
 * @return シーンマネージャ情報の取得
 */
IrrlichtSceneManager
IrrlichtBase::getSceneManager()
{
	return device ? IrrlichtSceneManager(device->getSceneManager()) : IrrlichtSceneManager();
}


/**
 * @return GUI環境情報の取得
 */
IrrlichtGUIEnvironment
IrrlichtBase::getGUIEnvironment()
{
	return device ? IrrlichtGUIEnvironment(device->getGUIEnvironment()) : IrrlichtGUIEnvironment();
}
