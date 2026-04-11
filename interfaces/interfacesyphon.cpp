#include "interfacesyphon.h"

#ifdef SYPHON_INSTALLED
#include "ui_interfacesyphon.h"

InterfaceSyphon::InterfaceSyphon(QWidget *parent) :
    NetworkInterface(parent),
    ui(new Ui::InterfaceSyphon),
    serverEnable(false),
    clientEnable(false),
    serverInit(false),
    clientInit(false),
    clientTextureOk(false),
    serverTexture(0),
    clientTexture(0),
    serverSyphon(0),
    clientSyphon(0) {
    ui->setupUi(this);
}

InterfaceSyphon::~InterfaceSyphon() {
    delete ui;
}

void InterfaceSyphon::initSyphonServer() {}
void InterfaceSyphon::releaseSyphonServer() {}
void InterfaceSyphon::createSyphonServer() {}
void InterfaceSyphon::createSyphonClient() {}
void InterfaceSyphon::publishTexture(int, int, int) {}
GLuint InterfaceSyphon::getTexture(QSizeF *size) {
    if(size)
        *size = QSizeF();
    return 0;
}

#else

InterfaceSyphon::InterfaceSyphon(QWidget *parent) :
    NetworkInterface(parent),
    ui(0) {
}

InterfaceSyphon::~InterfaceSyphon() {
}

#endif
