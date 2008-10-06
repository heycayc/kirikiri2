#ifndef IRRLICHTGUIENVIRONMENT_H
#define IRRLICHTGUIENVIRONMENT_H

#include <windows.h>
#include "tp_stub.h"
#include <irrlicht.h>

/**
 * GUI環境操作用クラス
 */
class IrrlichtGUIEnvironment {

protected:
	irr::gui::IGUIEnvironment *gui;
	
public:
	IrrlichtGUIEnvironment();
	IrrlichtGUIEnvironment(irr::gui::IGUIEnvironment *gui);
	IrrlichtGUIEnvironment(const IrrlichtGUIEnvironment &orig);
	~IrrlichtGUIEnvironment();

};

#endif

