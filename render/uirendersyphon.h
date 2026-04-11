#ifndef UIRENDERSYPHON_H
#define UIRENDERSYPHON_H

#ifdef USE_OPENGLWIDGET
#include <QOpenGLWidget>
#else
#include <QGLWidget>
#endif

class UiRenderSyphon {
public:
    explicit UiRenderSyphon();
    ~UiRenderSyphon();

public:
    void openPort();
    void publishTexture(GLuint textureId, int textureTarget, int width, int height);

protected:
    void *mSyphon;
};
