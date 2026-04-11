/*
    This file is part of IanniX, a graphical real-time open-source sequencer for digital art
    Copyright (C) 2010-2015 — IanniX Association

    Project Manager: Thierry Coduys (http://www.le-hub.org)
    Development:     Guillaume Jacquemin (https://www.buzzinglight.com)

    This file was written by Guillaume Jacquemin.

    IanniX is a free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "uirender.h"
#include <QStandardPaths>
#include <QElapsedTimer>
#include "ui_uirender.h"

UiRender::UiRender(QWidget *parent, void *share) :
    Render(parent, share),
    ui(new Ui::UiRender) {
    capturedFramesStart = false;

    setFocusPolicy(Qt::StrongFocus);
#ifdef USE_OPENGLWIDGET
    renderTextTextureIndex = 0;
    renderTextFont = OpenGlFont::getFont(Application::renderFont.family(), Qt::AlignLeft, Application::renderFont.pixelSize());
#endif

    //Initialize view
    ui->setupUi(this);
    setCursor(Qt::OpenHandCursor);
    grabGesture(Qt::PinchGesture);
    setAcceptDrops(true);

    //Syphon
    interfaceSyphon = new InterfaceSyphon();
    renderPreviewTextureInit = false;
    destinationTexture = workingTexture = 0;
    performanceMode = false;

    //Légende
    legendColor = Qt::white;
    legendSize = 30;

    //Initialisations
    documentToRender = 0;
    setDocument(0);
    setMouseTracking(true);
    isRemoving = false;
    mousePressed = false;
    mouseShift = false;
    mouseObjectDrag = false;
    selectedHover = 0;
    scale = 1;
    connect(&cameraPerspective, SIGNAL(triggered(bool)), SLOT(cameraPerspectiveChanged()));
    cameraPerspectiveChanged();

    //Refresh
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateGL()));

    snapBeforeKeyX = Application::mouseSnapX;
    snapBeforeKeyY = Application::mouseSnapY;
}
UiRender::~UiRender() {
    delete ui;
}
void UiRender::changeEvent(QEvent *event) {
#ifdef USE_OPENGLWIDGET
    return QOpenGLWidget::changeEvent(event);
#else
    return QGLWidget::changeEvent(event);
#endif
}


//Textures
bool UiRender::loadTexture(UiRenderTexture *texture, bool gl) {
    if(gl) {
        if(texture->filename.exists()) {
            glEnable(GL_TEXTURE_2D);
            glGenTextures(1, &(texture->texture));
            glBindTexture(GL_TEXTURE_2D, texture->texture);
#ifdef USE_OPENGLWIDGET
            QImage tex = QImage(texture->filename.absoluteFilePath()).convertToFormat(QImage::Format_RGBA8888).flipped(Qt::Vertical);
#else
            QImage tex = QGLWidget::convertToGLFormat(QImage(texture->filename.absoluteFilePath()));
#endif
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width(), tex.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.bits());
#ifdef Q_OS_MAC
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
#else
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#endif
            glDisable(GL_TEXTURE_2D);
            texture->originalSize = tex.size();
            texture->loaded = true;
            Render::textures->update();
            return true;
        }
    }
    else {
        Render::textures->insert(texture->name, texture);
        Render::textures->update();
        return true;
    }
    return false;
}
bool UiRender::removeTexture(const QString &name) {
    if(Render::textures->contains(name)) {
        delete Render::textures->value(name);
        Render::textures->remove(name);
        return true;
    }
    Render::textures->update();
    return false;
}

void UiRender::capture(qreal scaleFactor) {
    if(scaleFactor <= 0) {
#ifdef FFMPEG_INSTALLED
        //Encoder
        renderSize = QSize(800, 600);
        resize(renderSize);

        if(videoEncoder.isOk()) {
            timer->stop();
            qDebug("Fermeture de la video : %d", videoEncoder.close());
            timer->start(1000./50.);
        }
        else {
            setInterval(1000./25.);
            qDebug("Creation de la video : %d", videoEncoder.createFile("_test.avi", renderSize.width(), renderSize.height(), 5000000, 20, 25));
        }
#else
        if(capturedFramesStart) {
            capturedFramesStart = false;
            //Sauvegarde
            QString basePath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/IanniX_Capture_" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss") + "/";
            QDir().mkpath(basePath);
            quint16 index = 0;
            foreach(const QImage &capturedFrame, capturedFrames)
                capturedFrame.save(basePath + tr("Image_%1.png").arg(index++, 5, 10, QChar('0')));
            capturedFrames.clear();
        }
        else {
            capturedFrames.clear();
            capturedFramesStart = true;
        }
#endif
    }
    else
        captureFrame(scaleFactor);
}
bool UiRender::captureFrame(qreal scaleFactor, const QString &filename) {
    renderSize = size() * scaleFactor;
    Render::forceLists         = true;
    Render::forceTexture       = true;
    Render::forceFrustumInInit = true;

#ifdef QT4 //TODO
    if(filename.isEmpty()) {
        QPixmap picture = renderPixmap(renderSize.width(), renderSize.height());
        if(picture.isNull()) {
            picture = QPixmap::fromImage(grabFrameBuffer(false));
            (new UiMessageBox())->display(tr("Graphical card error"), tr("Due to hardware issue, the high resolution snapshot creation failed.\nA classical snapshot has been saved on your desktop."));
        }
        picture.save(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/IanniX_Capture_" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss") + ".png");
    } else {
        QDir().mkpath(QFileInfo(filename).absoluteDir().absolutePath());
        renderPixmap(renderSize.width(), renderSize.height()).save(filename);
    }
#else
    if(filename.isEmpty()) {
#ifdef USE_GLWIDGET
        QPixmap picture = QPixmap::fromImage(grabFrameBuffer());
#else
        QPixmap picture = QPixmap::fromImage(grabFramebuffer());
#endif
        (new UiMessageBox())->display(tr("Graphical card error"), tr("Due to hardware issue, the high resolution snapshot creation failed.\nA classical snapshot has been saved on your desktop."));
        picture.save(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/IanniX_Capture_" + QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss") + ".png");
    }/* else {
        QDir().mkpath(QFileInfo(filename).absoluteDir().absolutePath());
        renderPixmap(renderSize.width(), renderSize.height()).save(filename);
    }*/
#endif
    Render::forceLists         = false;
    Render::forceTexture       = false;
    Render::forceFrustumInInit = false;
    return true;
}

void UiRender::centerOn(const NxPoint & center, bool force) {
    Render::axisCenterDest = -center;
    if(force)
        Render::axisCenter = Render::axisCenterDest;
    setZoom();
}

void UiRender::rotateTo(const NxPoint & rotation, const NxPoint & rotationCenter, bool force) {
    Render::rotationDest = rotation;
    Render::rotationCenterDest = rotationCenter;
    emit(mouseRotationChanged(Render::rotationDest));
    if(force) {
        Render::rotation = Render::rotationDest;
        Render::rotationCenter = Render::rotationCenterDest;
    }
    setZoom();
}

void UiRender::setPerformanceMode(bool _performanceMode) {
    performanceMode = _performanceMode;
    if(performanceMode) {
        setParent(0);
        setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
        move(pos());
        resize(size());
        show();
        //activateWindow();
        //raise();
    }
    else {
        //activateWindow();
        //raise();
    }
}

//Initialize event
void UiRender::initializeGL() {
    QOpenGLFunctions glFuncs(QOpenGLContext::currentContext());

    //Flags
    glFuncs.glHint(GL_POINT_SMOOTH_HINT,   GL_NICEST);
    glFuncs.glHint(GL_LINE_SMOOTH_HINT,    GL_NICEST);
    glFuncs.glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    glFuncs.glEnable(GL_POINT_SMOOTH);
    glFuncs.glEnable(GL_LINE_SMOOTH);
    glFuncs.glEnable(GL_POLYGON_SMOOTH);
    glFuncs.glEnable(GL_BLEND);
    glFuncs.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glFuncs.glEnable(GL_BLEND);
    glFuncs.glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//Resize event
void UiRender::resizeGL(int width, int height) {
    //Set viewport
    glViewport(0, 0, (GLint)width, (GLint)height);

    if(Render::forceFrustumInInit)
        setFrustum();
}

void UiRender::setFrustum() {
    qreal width  = renderSize.width();
    qreal height = renderSize.height();
    //Calculate area
    Render::axisArea = NxRect(NxPoint(), NxSize(10, 10));
    if(width > height) {
        if(Render::axisArea.width() > -Render::axisArea.height())
            Render::axisArea.setHeight(-Render::axisArea.width() * height/width);
        else
            Render::axisArea.setWidth(-Render::axisArea.height() * width/height);
    }
    else {
        if(Render::axisArea.width() > -Render::axisArea.height())
            Render::axisArea.setHeight(-Render::axisArea.width() * height/width);
        else
            Render::axisArea.setWidth(-Render::axisArea.height() * width/height);
    }
    Render::axisArea.setWidth(Render::axisArea.width()   * Render::zoomLinear);
    Render::axisArea.setHeight(Render::axisArea.height() * Render::zoomLinear);
    Render::axisArea.translate(-NxPoint(Render::axisArea.size().width()/2, Render::axisArea.size().height()/2));
    Render::axisArea.translate(-Render::axisCenter);

    //Set axis
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if(cameraPerspective)
        glFrustum(Render::axisArea.left(), Render::axisArea.right(), Render::axisArea.bottom(), Render::axisArea.top(), 50, 650.0);
    else
        glOrtho(Render::axisArea.left(), Render::axisArea.right(), Render::axisArea.bottom(), Render::axisArea.top(), -650, 650);
    glMatrixMode(GL_MODELVIEW);
}

//Paint event
void UiRender::paintGL() {
    if(!isRemoving) {
        QMapIterator<QString, UiRenderTexture*> textureIterator(*Render::textures);
        while (textureIterator.hasNext()) {
            textureIterator.next();
            if((!textureIterator.value()->loaded) || (Render::forceTexture))
                loadTexture(textureIterator.value(), true);
        }

        //Intertial system
        Render::axisCenter = Render::axisCenter + (Render::axisCenterDest - Render::axisCenter) / 3;
        Render::zoomLinear = Render::zoomLinear + (Render::zoomLinearDest - Render::zoomLinear) / 3;
        Render::rotation = Render::rotation + (Render::rotationDest - Render::rotation) / 6;
        Render::rotationCenter = Render::rotationCenter + (Render::rotationCenterDest - Render::rotationCenter) / 6;
        //if(qAbs(UiRenderOptions::rotation.z() - UiRenderOptions::rotationDest.z()) > 360)
        //    UiRenderOptions::rotation.setZ(UiRenderOptions::rotationDest.z());
        translation = translation + (translationDest - translation) / 3;
        scale = scale + (scaleDest - scale) / 3;

        //Object sizes
        Render::objectSize = getAutoScale(Application::objectsAutosize/100.);

        if(!Render::forceFrustumInInit) {
            renderSize = size();
            setFrustum();
        }

        //Clear
        qglClearColor(Render::colors->value(Application::colorsPrefix() + "_background"));
        glClear(GL_COLOR_BUFFER_BIT);

        //Translation
        Render::axisArea.translate(Render::axisCenter);

        //Start drawing
        glPushMatrix();

        //First operations
        if(cameraPerspective)
            glTranslatef(0, 0, -150);

        if((Application::followId > 0) && (documentToRender) && (documentToRender->objects.contains(Application::followId)) && (documentToRender->objects.value(Application::followId)->getType() == ObjectsTypeCursor)) {
            NxCursor *object = (NxCursor*)documentToRender->objects.value(Application::followId);
            //rotationDest.setX(-object->getCurrentAngleRoll());
            //rotationDest.setY(-82 - object->getCurrentAnglePitch());
            Render::rotationDest.setZ(-object->getCurrentAngle().z() + 90);
            Render::rotation.setZ(Render::rotationDest.z());
            translationDest = -object->getCurrentPos();
            //scaleDest = 1 * 5;
        }

        glScalef(scale, scale, scale);

        glTranslatef(Render::rotationCenter.x(), Render::rotationCenter.y(), Render::rotationCenter.z());
        glRotatef(Render::rotation.y(), 1, 0, 0);
        glRotatef(Render::rotation.x(), 0, 1, 0);
        glRotatef(Render::rotation.z(), 0, 0, 1);
        glTranslatef(-Render::rotationCenter.x(), -Render::rotationCenter.y(), -Render::rotationCenter.z());

        glTranslatef(translation.x(), translation.y(), translation.z());

        if((Render::rotationDest.x() == 0) && (Render::rotationDest.y() == 0) && (Render::rotationDest.z() == 0))
            Application::allowSelection = true;
        else
            Application::allowSelection = false;


        //Start measure
        Transport::perfOpenGLRefreshTime += renderMeasure.elapsed() / 1000.0F;
        Transport::perfOpenGLCounterTime++;
        renderMeasure.restart();

        if(documentToRender) {
            //Background
            paintBackground();

            //Paint axis
            paintAxisGrid();

            //Paint selection
            paintSelection();

            //Draw objects
            //Browse groups
            for (NxGroup *group : documentToRender->groups) {
                glPushMatrix();

                //Group specific
                group->rotation    = group->rotation + (group->rotationDest - group->rotation) / 6;
                group->translation = group->translation + (group->translationDest - group->translation) / 3;
                group->scale       = group->scale + (group->scaleDest - group->scale) / 3;
                glTranslatef(group->translation.x(), group->translation.y(), group->translation.z());
                glRotatef(group->rotation.y(), 1, 0, 0);
                glRotatef(group->rotation.x(), 0, 1, 0);
                glRotatef(group->rotation.z(), 0, 0, 1);
                glScalef (group->scale, group->scale, group->scale);

                if(((!Application::current->isGroupSoloActive) && (group->isNotMuted())) || ((Application::current->isGroupSoloActive) && (group->isSolo())))
                    Render::paintThisGroup = true;
                else
                    Render::paintThisGroup = false;

                //Browse active/inactive objects
                for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {
                    //Browse all types of objects
                    for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {
                        //Browse objects
                        for (NxObject *object : group->objects[activityIterator][typeIterator]) {
                            //Draw the object
                            bool oldPaintThisGroup = Render::paintThisGroup;
                            if(!(((!Application::current->isObjectSoloActive) && (object->isNotMuted())) || ((Application::current->isObjectSoloActive) && (object->isSolo()))))
                                Render::paintThisGroup = false;
                            if(typeIterator == ObjectsTypeTrigger) {
                                object->paint();
                                //if(axisArea.contains(object->getPos()))
                                //    object->paint();
                            }
                            else
                                object->paint();
                            Render::paintThisGroup = oldPaintThisGroup;
                        }
                    }
                }

                glPopMatrix();
            }
#ifdef KINECT_INSTALLED
            if(Application::current->kinect)
                Application::current->kinect->paint();
#endif
        }
        glPopMatrix();

#ifdef FFMPEG_INSTALLED
        if(videoEncoder.isOk())
            qDebug("%d encode", videoEncoder.encodeImage(grabFrameBuffer()));
#endif

#ifdef SYPHON_INSTALLED
        //Export Syphon
        if(!interfaceSyphon->serverInit) {
            makeCurrent();
            interfaceSyphon->createSyphonServer();
        }
        if(interfaceSyphon->serverEnable) {
            glEnable(GL_TEXTURE_2D);
            if(!interfaceSyphon->serverTexture)
                glGenTextures(1, &interfaceSyphon->serverTexture);
            glBindTexture(GL_TEXTURE_2D, interfaceSyphon->serverTexture);
            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, renderSize.width() * OpenGlDrawing::dpi, renderSize.height() * OpenGlDrawing::dpi, 0);
            interfaceSyphon->publishTexture(GL_TEXTURE_2D, renderSize.width() * OpenGlDrawing::dpi, renderSize.height() * OpenGlDrawing::dpi);
            glDisable(GL_TEXTURE_2D);
        }

        //Import Syphon
        if(!interfaceSyphon->clientInit) {
            makeCurrent();
            interfaceSyphon->createSyphonClient();
            Render::textures->insert("syphon", new UiRenderTexture("syphon", QFileInfo(), NxRect(-4, 4, 8, -8)));
        }
        if(interfaceSyphon->clientEnable) {
            Render::textures->value("syphon")->texture = interfaceSyphon->getTexture(&Render::textures->value("syphon")->originalSize);
            Render::textures->value("syphon")->loaded = Render::textures->value("syphon")->isSyphon = true;
        }
#endif

        //Mode performance preview
        if((performanceMode) && (Application::current->getPerformancePreview()) && (Application::current->getRenderPreview())) {
            glEnable(GL_TEXTURE_2D);
            if(!renderPreviewTextureInit) {
                glGenTextures(1, &renderPreviewTexture);
                renderPreviewTextureInit = true;
            }
            glBindTexture(GL_TEXTURE_2D, renderPreviewTexture);
            glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, renderSize.width() * OpenGlDrawing::dpi, renderSize.height() * OpenGlDrawing::dpi, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glDisable(GL_TEXTURE_2D);
            Application::current->getRenderPreview()->paintPreview(this, renderPreviewTexture, renderSize * OpenGlDrawing::dpi);
        }
        if(capturedFramesStart)
#ifdef USE_GLWIDGET
            capturedFrames << grabFrameBuffer();
#else
            capturedFrames << grabFramebuffer();
#endif
    }
}


//Draw background
void UiRender::paintBackground() {
    if(Render::textures->contains("background")) {
        UiRenderTexture *texture = Render::textures->value("background");
        if((texture) && (texture->loaded) && (texture->mapping.width() != 0) && (texture->mapping.height() != 0)) {
            if(texture->isSyphon) {
                glEnable(GL_TEXTURE_RECTANGLE_ARB);
                glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture->texture);
                glBegin(GL_QUADS);
                qglColor(Render::colors->value("background_texture_tint"));
                glLineWidth(OpenGlDrawing::dpi);
                glTexCoord2d(0, 0); glVertex3f(texture->mapping.left() , texture->mapping.bottom(), 0);
                glTexCoord2d(texture->originalSize.width(), 0); glVertex3f(texture->mapping.right(), texture->mapping.bottom(), 0);
                glTexCoord2d(texture->originalSize.width(), texture->originalSize.height()); glVertex3f(texture->mapping.right(), texture->mapping.top(), 0);
                glTexCoord2d(0, texture->originalSize.height()); glVertex3f(texture->mapping.left() , texture->mapping.top(), 0);
                glEnd();
                glDisable(GL_TEXTURE_RECTANGLE_ARB);
            }
            else {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texture->texture);
                glBegin(GL_QUADS);
                qglColor(Render::colors->value("background_texture_tint"));
                glLineWidth(OpenGlDrawing::dpi);
                glTexCoord2d(0, 0); glVertex3f(texture->mapping.left() , texture->mapping.bottom(), 0);
                glTexCoord2d(1, 0); glVertex3f(texture->mapping.right(), texture->mapping.bottom(), 0);
                glTexCoord2d(1, 1); glVertex3f(texture->mapping.right(), texture->mapping.top(), 0);
                glTexCoord2d(0, 1); glVertex3f(texture->mapping.left() , texture->mapping.top(), 0);
                glEnd();
                glDisable(GL_TEXTURE_2D);
            }
        }
    }
}


//Draw grid axis
void UiRender::paintAxisGrid() {
    if(Application::paintAxisGrid) {
        //Draw axis
        Render::axisArea.translate(-Render::axisCenter);
        qreal gridFactor = 10;

        for(qreal x = 0 ; x < ceil(Render::axisArea.right()*gridFactor) ; x += Render::axisGrid) {
            if((x == 0) && (Application::paintAxisGrid)) {
                if(Application::mouseSnapX)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_axisSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_axis"));
                glLineWidth(OpenGlDrawing::dpi * 2);
            }
            else {
                if(Application::mouseSnapX)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_gridSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_grid"));
                glLineWidth(OpenGlDrawing::dpi);
            }
            glBegin(GL_LINES);
            glVertex3f(x, Render::axisArea.bottom()*gridFactor, 0);
            glVertex3f(x, Render::axisArea.top()*gridFactor, 0);
            glEnd();
        }
        for(qreal x = 0 ; x > floor(Render::axisArea.left()*gridFactor) ; x -= Render::axisGrid) {
            if((x == 0) && (Application::paintAxisGrid)) {
                if(Application::mouseSnapX)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_axisSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_axis"));
                glLineWidth(OpenGlDrawing::dpi * 2);
            }
            else {
                if(Application::mouseSnapX)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_gridSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_grid"));
                glLineWidth(OpenGlDrawing::dpi);
            }
            glBegin(GL_LINES);
            glVertex3f(x, Render::axisArea.bottom()*gridFactor, 0);
            glVertex3f(x, Render::axisArea.top()*gridFactor, 0);
            glEnd();
        }
        for(qreal y = 0 ; y < ceil(Render::axisArea.top()*gridFactor) ; y += Render::axisGrid) {
            if((y == 0) && (Application::paintAxisGrid)) {
                if(Application::mouseSnapY)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_axisSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_axis"));
                glLineWidth(OpenGlDrawing::dpi * 2);
            }
            else {
                if(Application::mouseSnapY)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_gridSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_grid"));
                glLineWidth(OpenGlDrawing::dpi);
            }
            glBegin(GL_LINES);
            glVertex3f(Render::axisArea.left()*gridFactor, y, 0);
            glVertex3f(Render::axisArea.right()*gridFactor, y, 0);
            glEnd();
        }
        for(qreal y = 0 ; y > floor(Render::axisArea.bottom()*gridFactor) ; y -= Render::axisGrid) {
            if((y == 0) && (Application::paintAxisGrid)) {
                if(Application::mouseSnapY)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_axisSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_axis"));
                glLineWidth(OpenGlDrawing::dpi * 2);
            }
            else {
                if(Application::mouseSnapY)
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_gridSnap"));
                else
                    qglColor(Render::colors->value(Application::colorsPrefix() + "_grid"));
                glLineWidth(OpenGlDrawing::dpi);
            }
            glBegin(GL_LINES);
            glVertex3f(Render::axisArea.left()*gridFactor, y, 0);
            glVertex3f(Render::axisArea.right()*gridFactor, y, 0);
            glEnd();
        }

        Render::axisArea.translate(Render::axisCenter);
    }
}

//Draw selection
void UiRender::paintSelection() {
    //Axis color
    qglColor(Render::colors->value(Application::colorsPrefix() + "_gui_selection"));
    glLineWidth(OpenGlDrawing::dpi);

    //Draw axis
    glBegin(GL_QUADS);
    glVertex3f(Render::selectionArea.left(),  Render::selectionArea.top(), 0);
    glVertex3f(Render::selectionArea.right(), Render::selectionArea.top(), 0);
    glVertex3f(Render::selectionArea.right(), Render::selectionArea.bottom(), 0);
    glVertex3f(Render::selectionArea.left(),  Render::selectionArea.bottom(), 0);
    glVertex3f(Render::selectionArea.left(),  Render::selectionArea.top(), 0);
    glEnd();
}


void UiRender::wheelEvent(QWheelEvent *event) {
    bool mouse3D = ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) && !((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier);

    //Zoom calculation
    if(mouse3D) {
        scaleDest = qMax((qreal)0, scale - (qreal)event->angleDelta().y() / 150.0F);
        //refresh();
    }
    else if(event->modifiers() & Qt::ShiftModifier) Application::current->execute(QString("%1 %2").arg(COMMAND_ZOOM).arg(Render::zoomValue - (qreal)event->angleDelta().y() / 3.0F), ExecuteSourceGui);
    else                                            Application::current->execute(QString("%1 %2").arg(COMMAND_ZOOM).arg(Render::zoomValue - (qreal)event->angleDelta().y() / 15.0F), ExecuteSourceGui);
}
void UiRender::mousePressEvent(QMouseEvent *event) {
    //Save state when pressed
    mousePressedRawPos = NxPoint(event->pos().x(), event->pos().y());
    //Mouse position
    mousePressedAreaPosNoCenter = NxPoint((event->pos().x() - (qreal)size().width()/2) / (qreal)size().width() * Render::axisArea.width(), (event->pos().y() - (qreal)size().height()/2) / (qreal)size().height() * Render::axisArea.height());
    mousePressedAreaPos = mousePressedAreaPosNoCenter - Render::axisCenter;
    if(Application::mouseSnapX) {
        mousePressedAreaPosNoCenter.setX(qRound(mousePressedAreaPosNoCenter.x() / Render::axisGrid) * Render::axisGrid);
        mousePressedAreaPos.setX(qRound(mousePressedAreaPos.x() / Render::axisGrid) * Render::axisGrid);
    }
    if(Application::mouseSnapY) {
        mousePressedAreaPosNoCenter.setY(qRound(mousePressedAreaPosNoCenter.y() / Render::axisGrid) * Render::axisGrid);
        mousePressedAreaPos.setY(qRound(mousePressedAreaPos.y() / Render::axisGrid) * Render::axisGrid);
    }

    mousePressed = true;
    mousePressedAxisCenter = Render::axisCenter;
    mouseControl = (event->modifiers() & Qt::ControlModifier);
    mouseShift = (event->modifiers() & Qt::ShiftModifier);
    const bool mouseAlt = (event->modifiers() & Qt::AltModifier);
    rotationDrag = Render::rotation;
    translationDrag = translation;

    if(cursor().shape() == Qt::BlankCursor)
        return;

    // Alt+Click on curve: create cursor and attach to curve (Ctrl is reserved for duplicate-drag).
    if(mouseAlt && selectedHover && selectedHover->getType() == ObjectsTypeCurve && !Render::editing) {
        NxCurve *curve = (NxCurve*)selectedHover;
        quint16 cursorId = Application::current->execute("add cursor auto", ExecuteSourceGui).toUInt();
        Application::current->execute(QString("%1 %2 %3").arg(COMMAND_CURSOR_CURVE).arg(cursorId).arg(curve->getId()), ExecuteSourceGui);

        // Use raw click position (without snap) for precise cursor placement.
        NxPoint rawMouseAreaPosNoCenter(
            (event->pos().x() - (qreal)size().width()/2) / (qreal)size().width() * Render::axisArea.width(),
            (event->pos().y() - (qreal)size().height()/2) / (qreal)size().height() * Render::axisArea.height());
        NxPoint rawMouseAreaPos = rawMouseAreaPosNoCenter - Render::axisCenter;

        // Find nearest hit on curve and position cursor by time percentage.
        NxPoint collisionPoint;
        qreal clickOffset = curve->intersects(NxRect(rawMouseAreaPos - NxPoint(0.1, 0.1), rawMouseAreaPos + NxPoint(0.1, 0.1)), &collisionPoint);

        if(clickOffset >= 0 && clickOffset <= 1.0) {
            Application::current->execute(QString("%1 %2 %3").arg(COMMAND_CURSOR_TIME_PERCENT).arg(cursorId).arg(clickOffset), ExecuteSourceGui);
        } else {
            Application::current->execute(QString("%1 %2 %3").arg(COMMAND_CURSOR_TIME_PERCENT).arg(cursorId).arg(0.5), ExecuteSourceGui);
        }

        // Activate cursor (make it ready to work)
        Application::current->execute(QString("%1 %2 0 0 1").arg(COMMAND_CURSOR_START).arg(cursorId), ExecuteSourceGui);
        
        changeStatus(QString("Cursor %1 created on curve %2 (Alt+Click)").arg(cursorId).arg(curve->getId()));
        return;
    }

    //Start area selection
    if((Render::editing) && (Application::allowSelection)) {
        for (NxObject *selected : selection)
            selected->setSelected(false);
        selection.clear();
        //selectedHover->setSelected(true);   /// select this one
        //selectionAdd(selectedHover);
        if(Render::editingMode == EditingModeFree) {
            //emit(editingStart(mousePressedAreaPos));
            if(Render::editingFirstPoint)
                emit(editingStart(mousePressedAreaPos));
            else
                emit(editingMove(mousePressedAreaPos, true, false));
        }
        else if(Render::editingMode == EditingModePoint) {
            if(Render::editingFirstPoint)
                emit(editingStart(mousePressedAreaPos));
            else
                emit(editingMove(mousePressedAreaPos, true, false));
        }
        else if(Render::editingMode == EditingModeTriggers)
            emit(editingStart(mousePressedAreaPos));
        else if(Render::editingMode == EditingModeCircle)
            emit(editingStart(mousePressedAreaPos));
        Render::editingFirstPoint = false;
    }
    /*
    else if(!Application::allowSelection) {
        for (NxObject *selected : selection)
            selected->setSelected(false);
        selection.clear();
        if(selectedHover) {
            selectedHover->setSelectedHover(false);
            selectedHover = 0;
        }
    }
    */
    else {
        if(mouseShift) {
            Render::selectionArea.setTopLeft(mousePressedAreaPos);
            Render::selectionArea.setBottomRight(mousePressedAreaPos);
            if(cursor().shape() != Qt::BlankCursor)
                setCursor(Qt::CrossCursor);
        } else if (selectedHover) {
            if (!selectedHover->getSelected()) {
                for (NxObject *selected : selection)
                    selected->setSelected(false);
                selection.clear();
                selectedHover->setSelected(true);
                selectionAdd(selectedHover);
            }
        }
        else if((!selectedHover) && (cursor().shape() != Qt::BlankCursor))
            setCursor(Qt::ClosedHandCursor);
    }
}
void UiRender::mouseReleaseEvent(QMouseEvent *event) {
    //Edition
    if((Render::editing) && (Application::allowSelection) && (cursor().shape() != Qt::BlankCursor)) {
        /*
        if(editingMode == EditingModeFree) {
            emit(editingStop());
            editing = false;
        }
        */
    }
    else if((cursor().shape() != Qt::BlankCursor) && (Application::allowSelection)) {
        //Copy area selection
        for (NxObject *selected : selectionRect)
            selectionAdd(selected);
        selectionRect.clear();

        //End of drag
        for (NxObject *object : selection)
            object->dragStop();
        if(selectedHover)
            selectedHover->dragStop();

        emit(selectionChanged());

        //Simple clic
        if(mousePressedRawPos == NxPoint(event->pos().x(), event->pos().y())) {
            if(selectedHover) {
                //Click on an object
                if(!mouseShift)
                    selectionClear();

                //Invert selection
                if(selectedHover->getSelected()) {
                    selectedHover->setSelected(false);
                    selection.removeOne(selectedHover);
                }
                else
                    selectionAdd(selectedHover);
            }
            else if((!selectedHover) && (!mouseShift))
                //Click on the workspace
                selectionClear();
        }
    }

    //Release states
    mousePressed = false;
    mouseShift = false;
    mouseObjectDrag = false;
    Render::selectionArea.setSize(NxSize(0, 0));
    if((cursor().shape() != Qt::BlankCursor) && !((Render::editing) && ((Render::editingMode == EditingModeFree) || (Render::editingMode == EditingModePoint) || (Render::editingMode == EditingModeTriggers) || (Render::editingMode == EditingModeCircle)))) {
        if(mouseShift)
            setCursor(Qt::CrossCursor);
        else
            setCursor(Qt::OpenHandCursor);
    }
}
void UiRender::mouseMoveEvent(QMouseEvent *event) {
    //Mouse position
    bool mouse3D = ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) && !((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier);
    NxPoint mousePosNoCenter = NxPoint((event->pos().x() - (qreal)size().width()/2) / (qreal)size().width() * Render::axisArea.width(), (event->pos().y() - (qreal)size().height()/2) / (qreal)size().height() * Render::axisArea.height());
    NxPoint mousePos = mousePosNoCenter - Render::axisCenter, mousePosBackup = mousePos;
    NxPoint deltaMouseRaw = NxPoint(event->pos().x(), event->pos().y()) - mousePressedRawPos;

    if(Application::mouseSnapX)  mousePos.setX(qRound(mousePos.x() / Render::axisGrid) * Render::axisGrid);
    if(Application::mouseSnapY)  mousePos.setY(qRound(mousePos.y() / Render::axisGrid) * Render::axisGrid);
    bool noSelection = true;
    emit(mousePosChanged(mousePos));

    if((Render::editing) && (cursor().shape() != Qt::BlankCursor) && (Application::allowSelection)) {
        if((mousePressed) && (Render::editingMode == EditingModeFree))
            emit(editingMove(mousePos, true, mousePressed));
        else if((!Render::editingFirstPoint) && ((Render::editingMode == EditingModeFree) || (Render::editingMode == EditingModePoint))){
            emit(editingMove(mousePos, false, mousePressed));
        }
    }
    else {
        //Cursor
        if((cursor().shape() != Qt::BlankCursor) && (Application::allowSelection)) {
            if((mouseShift) && (mousePressed))              setCursor(Qt::CrossCursor);
            else if((selectedHover) && (!mousePressed))     setCursor(Qt::PointingHandCursor);
            else if((!selectedHover) && (mousePressed))     setCursor(Qt::ClosedHandCursor);
            else                                            setCursor(Qt::OpenHandCursor);
        }

        //Mouse pressed
        if(mousePressed) {
            if(mouse3D) {
                if(mouseShift)  translationDest = translationDrag + 10 * NxPoint(deltaMouseRaw.x() / (qreal)size().width(), deltaMouseRaw.y() / (qreal)size().height());
                else {
                    NxPoint newRotation = rotationDrag + 360 * NxPoint(0, deltaMouseRaw.y() / (qreal)size().height(), deltaMouseRaw.x() / (qreal)size().width());
                    Application::current->execute(QString("%1 %2 %3 %4").arg(COMMAND_ROTATE).arg(newRotation.x()).arg(newRotation.y()).arg(newRotation.z()), ExecuteSourceGui);
                }
            }
            else if((mouseShift) && (Application::allowSelection) && (cursor().shape() != Qt::BlankCursor)) {
                Render::selectionArea.setBottomRight(mousePos);
            }
            else if((!Application::allowLockPos) && (Application::allowSelection) && ((selection.contains(selectedHover)) || (selectedHover) || (mouseObjectDrag)) && (cursor().shape() != Qt::BlankCursor)) {
                if(!mouseObjectDrag) {
                    mouseObjectDrag = true;
                    Application::current->pushSnapshot();

                    if(mouseControl && !mouseShift) {
                        if(duplicateSelectionForDrag())
                            changeStatus(tr("Duplicated selection (Ctrl+drag)"));
                    }

                    for (NxObject *object : selection)
                        object->dragStart(mousePos, selection.count() > 1);
                }
                NxPoint dragTranslation = mousePos - mousePressedAreaPos;
                if(selectedHover) {
                    if(Application::mouseSnapX)  dragTranslation.setX(qRound((selectedHover->getPosDrag().x() + dragTranslation.x()) / Render::axisGrid) * Render::axisGrid - selectedHover->getPosDrag().x());
                    if(Application::mouseSnapY)  dragTranslation.setY(qRound((selectedHover->getPosDrag().y() + dragTranslation.y()) / Render::axisGrid) * Render::axisGrid - selectedHover->getPosDrag().y());
                }
                for (NxObject *object : selection)
                    object->drag(dragTranslation, mousePos, selection.count() > 1);
            }
            else if(!mouseObjectDrag) {
                //New center
                NxPoint newCenter = -(mousePressedAxisCenter + NxPoint((mousePosNoCenter - mousePressedAreaPosNoCenter).x(), (mousePosNoCenter - mousePressedAreaPosNoCenter).y()));
                Application::current->execute(QString("%1 %2 %3 0").arg(COMMAND_CENTER).arg(newCenter.x()).arg(newCenter.y()), ExecuteSourceGui);
            }
        }

        mousePos = mousePosBackup;
        if((documentToRender) && (cursor().shape() != Qt::BlankCursor) && (Application::allowSelection) && (!Application::allowLockPos)) {
            UiRenderSelection eligibleSelection;

            //Browse documents
            for (NxGroup *group : documentToRender->groups) {
                //Is groups visible ?
                if((((!Application::current->isGroupSoloActive) && (group->isNotMuted())) || ((Application::current->isGroupSoloActive) && (group->isSolo())))) {
                    //Browse active/inactive objects
                    for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {
                        //Browse all types of objects
                        for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {
                            //Are objects visible ?
                            if(((typeIterator == ObjectsTypeCursor) && (Application::allowSelectionCursors)) || ((typeIterator == ObjectsTypeCurve) && (Application::allowSelectionCurves)) || ((typeIterator == ObjectsTypeTrigger) && (Application::allowSelectionTriggers))) {
                                //Browse objects
                                for (NxObject *object : group->objects[activityIterator][typeIterator]) {
                                    //Is Z visible ?
                                    if(((!Application::current->isObjectSoloActive) && (object->isNotMuted())) || ((Application::current->isObjectSoloActive) && (object->isSolo()))) {
                                        //Check selection
                                        if(object->isMouseHover(mousePos)) {
                                            if(object->getType() == ObjectsTypeCurve)
                                                eligibleSelection.prepend(object);
                                            else
                                                eligibleSelection.append(object);
                                        }

                                        //Add the object to selection if click+shift
                                        if((mouseShift) || (mouseControl)) {
                                            NxRect objectBoundingRect = object->getBoundingRect();
                                            if(objectBoundingRect.width() == 0)  objectBoundingRect.setWidth(0.001);
                                            if(objectBoundingRect.height() == 0) objectBoundingRect.setHeight(0.001);
                                            if(Render::selectionArea.intersects(objectBoundingRect)) {
                                                selectionRect.append(object);
                                                object->setSelected(true);
                                            }
                                            else {
                                                selectionRect.removeOne(object);
                                                if(!selection.contains(object))
                                                    object->setSelected(false);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for (NxObject *object : eligibleSelection) {
                bool selected = false;
                if(object->getType() == ObjectsTypeCurve) {
                    NxCurve *curve = (NxCurve*)object;
                    if(curve->isMouseHover(mousePos)) {
                        selected = true;
                        if(curve->getSelected()) {
                            qreal squareSize = (0.15 * Render::zoomLinear) / 2;
                            curve->isOnPathPoint(NxRect(mousePos - NxPoint(squareSize, squareSize, squareSize) - curve->getPos(), mousePos + NxPoint(squareSize, squareSize, squareSize) - curve->getPos()));
                        }
                    }
                }
                else if(object->isMouseHover(mousePos))
                    selected = true;

                //Add the object to selection
                if((selected) && (!mousePressed)) {
                    if((selectedHover) && (selectedHover != object))
                        selectedHover->setSelectedHover(false);
                    selectedHover = object;
                    selectedHover->setSelectedHover(true);
                    emit(selectionChanged());
                    noSelection = false;
                }
            }
            if((noSelection) && (selectedHover) && ((!mousePressed))) {
                selectedHover->setSelectedHover(false);
                selectedHover = 0;
            }
        }
    }

    if(((selectedHover) && (selectedHover->getType() == ObjectsTypeCurve)) || ((Render::editing) && ((Render::editingMode == EditingModeFree) || (Render::editingMode == EditingModePoint) || (Render::editingMode == EditingModeCircle))))
        changeStatus(curveStatusTip);
    else if((selectedHover) && (selectedHover->getType() == ObjectsTypeCursor))
        changeStatus(cursorStatusTip);
    else if(((selectedHover) && (selectedHover->getType() == ObjectsTypeTrigger)) || ((Render::editing) && (Render::editingMode == EditingModeTriggers)))
        changeStatus(triggerStatusTip);
    else
        changeStatus(defaultStatusTip);
}
void UiRender::changeStatus(const QString &_statusTip) {
    if(_statusTip != statusTip()) {
        setStatusTip(_statusTip);
        UiHelp::statusHelp(this);
    }
}

void UiRender::mouseDoubleClickEvent(QMouseEvent *event) {
    bool mouse3D = ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) && !((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier);
    bool mouseControl = event->modifiers() & Qt::ControlModifier;
    bool mouseShift   = event->modifiers() & Qt::ShiftModifier;

    if(mouse3D) {
        cameraPerspectiveChanged();
        Application::current->execute(QString("%1 0 0 0").arg(COMMAND_ROTATE), ExecuteSourceGui);
        translationDest = NxPoint();
        //refresh();
    }
    else {
        if(cursor().shape() == Qt::BlankCursor)
            return;

        else if((Render::editing) && (Application::allowSelection)) {
            emit(editingStop());
            Render::editing = false;
        }
        else if(selectedHover) {
            if((Application::allowSelection) && (selectedHover->getType() == ObjectsTypeCurve)) {
                NxCurve *curve = (NxCurve*)selectedHover;
                curve->addMousePointAt(mousePressedAreaPos, mouseControl);
            }
            else if(mouseShift) {
                const int repeatCount = mouseControl ? 2 : 1;
                for(int i = 0; i < repeatCount; i++)
                    Application::current->execute(QString("%1 %2").arg(COMMAND_TRIG).arg(selectedHover->getId()), ExecuteSourceGui);
            }
            else if((selectedHover->getType() == ObjectsTypeTrigger) || (selectedHover->getType() == ObjectsTypeCursor))
                Application::current->openMessageEditor();
        }
    }
}

bool UiRender::duplicateSelectionForDrag() {
    if(selection.isEmpty() || !documentToRender)
        return false;

    // Reuse the same serialization path as copy/paste so curve JS blocks
    // (points arrays/loops) are preserved when duplicating by drag.
    QString duplicateScript;
    for (NxObject *object : selection) {
        if(object->getType() != ObjectsTypeCursor)
            duplicateScript += object->serialize();
    }

    // Cursor-only selection fallback.
    if(duplicateScript.isEmpty()) {
        for (NxObject *object : selection)
            duplicateScript += object->serialize();
    }

    if(duplicateScript.isEmpty())
        return false;

    QSet<quint16> idsBefore;
    for(QHash<quint16, NxObject*>::const_iterator it = documentToRender->objects.constBegin(); it != documentToRender->objects.constEnd(); ++it)
        idsBefore.insert(it.key());

    documentToRender->source = ExecuteSourceGui;
    documentToRender->scriptEvaluate(duplicateScript, true);

    QList<quint16> newIds;
    for(QHash<quint16, NxObject*>::const_iterator it = documentToRender->objects.constBegin(); it != documentToRender->objects.constEnd(); ++it)
        if(!idsBefore.contains(it.key()))
            newIds << it.key();

    if(newIds.isEmpty())
        return false;

    for (NxObject *object : selection)
        object->setSelected(false);
    selection.clear();
    selectedHover = 0;

    for (const quint16 newId : newIds) {
        NxObject *newObject = (NxObject*)Application::current->getObjectById(newId);
        if(newObject) {
            selectionAdd(newObject);
            if(!selectedHover)
                selectedHover = newObject;
        }
    }

    emit(selectionChanged());
    return !selection.isEmpty();
}

void UiRender::triggerSelectedTriggers(int repeatCount) {
    if(repeatCount < 1)
        repeatCount = 1;

    for(int repeat = 0; repeat < repeatCount; repeat++) {
        for (NxObject *object : selection) {
            if(object->getType() == ObjectsTypeTrigger)
                Application::current->execute(QString("%1 %2").arg(COMMAND_TRIG).arg(object->getId()), ExecuteSourceGui);
        }
    }
}

void UiRender::keyPressEvent(QKeyEvent *event) {
    qreal translationUnit = 0.1;

    if(((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) && ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier)) {
        snapBeforeKeyX = Application::mouseSnapX;
        snapBeforeKeyY = Application::mouseSnapY;
        Application::mouseSnapY = Application::mouseSnapX = true;
    }

    if((event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier)
        translationUnit = 1;

    // Keyboard note input for selected triggers (layout-independent physical keys)
    static int currentOctave = 4; // Default octave (C4 = 60)
    if(selection.count() > 0) {
        // Check if all selected objects are triggers
        bool allTriggers = true;
        foreach(const NxObject *object, selection) {
            if(object->getType() != ObjectsTypeTrigger) {
                allTriggers = false;
                break;
            }
        }
        
        if(allTriggers) {
            int noteOffset = -1;
            bool octaveChanged = false;
            bool velocityChanged = false;
            int velocityDelta = 0;
            bool durationChanged = false;
            int durationDelta = 0;
            
            // Use physical scan codes for keyboard layout independence
            quint32 scanCode = event->nativeScanCode();
            
            // Debug: log scan code for troubleshooting keyboard layout issues
            qDebug("[KEYBOARD] Scan code: %d, Key: %s", scanCode, qPrintable(QKeySequence(event->key()).toString()));
            
            // Linux scan codes (QWERTY physical positions, independent of layout)
            // Bottom row: ASDFGHJK = white keys (C D E F G A B C)
            // Top row: WETYUIO = black keys (C# D# F# G# A#)
            // Bottom row: ZX = octave control, CV = velocity control, NM = duration control
            switch(scanCode) {
                // White keys (ASDFGHJK)
                case 38: noteOffset = 0; break;   // A = C
                case 39: noteOffset = 2; break;   // S = D
                case 40: noteOffset = 4; break;   // D = E
                case 41: noteOffset = 5; break;   // F = F
                case 42: noteOffset = 7; break;   // G = G
                case 43: noteOffset = 9; break;   // H = A
                case 44: noteOffset = 11; break;  // J = B
                case 45: noteOffset = 12; break;  // K = C (next octave)
                
                // Black keys (WETYUIO)
                case 25: noteOffset = 1; break;   // W = C#
                case 26: noteOffset = 3; break;   // E = D#
                case 27: noteOffset = 3; break;   // R = D# (alternative for some layouts)
                case 28: noteOffset = 6; break;   // T = F#
                case 29: noteOffset = 8; break;   // Y = G#
                case 30: noteOffset = 10; break;  // U = A#
                case 31: noteOffset = 12; break;  // I = C (next octave)
                case 32: noteOffset = 12; break;  // O = C (next octave, alternative)
                
                // Octave control (Z/X)
                case 52: // Z - decrease octave
                    if(currentOctave > -1) {
                        currentOctave--;
                        octaveChanged = true;
                    }
                    break;
                case 53: // X - increase octave
                    if(currentOctave < 9) {
                        currentOctave++;
                        octaveChanged = true;
                    }
                    break;
                
                // Velocity control (C/V)
                case 54: // C - decrease velocity
                    velocityDelta = -5;
                    velocityChanged = true;
                    break;
                case 55: // V - increase velocity
                    velocityDelta = +5;
                    velocityChanged = true;
                    break;
                
                // Duration control (N/M)
                case 57: // N - decrease duration
                    durationDelta = -50;
                    durationChanged = true;
                    break;
                case 58: // M - increase duration
                    durationDelta = +50;
                    durationChanged = true;
                    break;
                
                default:
                    // Unknown scan code - do nothing to prevent unintended behavior
                    break;
            }
            
            // Only process if we actually changed something
            if(!octaveChanged && !velocityChanged && !durationChanged && noteOffset < 0) {
                // Not a music input key, let other handlers process it
                // Don't accept the event
                goto skipMusicInput;
            }
            
            if(octaveChanged) {
                changeStatus(QString("Octave: %1").arg(currentOctave));
                event->accept();
                return;
            }
            
            if(velocityChanged) {
                // Check if any selected trigger has MIDI message
                bool hasMidiTrigger = false;
                foreach(const NxObject *object, selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    if(trigger->getMessagePatternsStr().contains("midi://")) {
                        hasMidiTrigger = true;
                        break;
                    }
                }
                if(!hasMidiTrigger) {
                    changeStatus(QString("No MIDI triggers selected"));
                    event->accept();
                    return;
                }
                
                Application::current->pushSnapshot();
                // Change velocity for all selected triggers
                // NOTE: This switches from DYNAMIC to FIXED velocity
                bool hadDynamicVelocity = false;
                for (NxObject *object : selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    QString currentMessage = trigger->getMessagePatternsStr();
                    
                    int channel = 1, note = 60, velocity = 100, duration = 500;
                    bool isDynamic = false;
                    if(currentMessage.contains("midi://")) {
                        QStringList parts = currentMessage.split(" ", Qt::SkipEmptyParts);
                        // Format: "interval, midi://midi_out/note channel note velocity duration"
                        // parts[0]=interval, parts[1]=midi://, parts[2]=channel, parts[3]=note, parts[4]=velocity, parts[5]=duration
                        if(parts.count() >= 6) {
                            channel = parts[2].toInt();
                            note = parts[3].toInt();
                            
                            // Check if velocity is dynamic (contains trigger_)
                            if(parts[4].contains("trigger_")) {
                                velocity = 100; // Default for switching from dynamic to fixed
                                isDynamic = true;
                                hadDynamicVelocity = true;
                            } else {
                                velocity = parts[4].toInt();
                            }
                            
                            // Preserve duration (could be dynamic or fixed)
                            if(parts[5].contains("trigger_")) {
                                duration = 500; // Keep default
                            } else {
                                duration = parts[5].toInt();
                                if(duration <= 0) duration = 500;
                            }
                        }
                    }
                    
                    int oldVelocity = velocity;
                    velocity = qBound(1, velocity + velocityDelta, 127);
                    
                    // Create FIXED velocity command - use /note for fixed numeric values
                    QString midiCommand = QString("midi://midi_out/note %1 %2 %3 %4")
                        .arg(channel).arg(note).arg(velocity).arg(duration);
                    
                    Application::current->execute(QString("%1 %2 21, %3").arg(COMMAND_MESSAGE).arg(trigger->getId()).arg(midiCommand), ExecuteSourceGui);
                    qDebug("[VELOCITY] Trigger %d: %s %d -> %d", trigger->getId(), isDynamic ? "dynamic" : "fixed", oldVelocity, velocity);
                }
                // Show average velocity across all selected triggers
                int avgVelocity = 0;
                int count = 0;
                for (NxObject *object : selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    QString msg = trigger->getMessagePatternsStr();
                    if(msg.contains("midi://")) {
                        QStringList parts = msg.split(" ", Qt::SkipEmptyParts);
                        if(parts.count() >= 6) {
                            // parts[4] might be a number or "trigger_value_x" string
                            bool ok;
                            int vel = parts[4].toInt(&ok);
                            if(ok) {
                                avgVelocity += vel;
                                count++;
                            }
                        }
                    }
                }
                if(count > 0) avgVelocity /= count;
                QString statusMsg = QString("Velocity: %1%2 (now %3)")
                    .arg(velocityDelta > 0 ? "+" : "").arg(velocityDelta).arg(avgVelocity);
                if(hadDynamicVelocity) {
                    statusMsg += " [switched to FIXED]";
                }
                changeStatus(statusMsg);
                event->accept();
                return;
            }
            
            if(durationChanged) {
                // Check if any selected trigger has MIDI message
                bool hasMidiTrigger = false;
                foreach(const NxObject *object, selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    if(trigger->getMessagePatternsStr().contains("midi://")) {
                        hasMidiTrigger = true;
                        break;
                    }
                }
                if(!hasMidiTrigger) {
                    changeStatus(QString("No MIDI triggers selected"));
                    event->accept();
                    return;
                }
                
                Application::current->pushSnapshot();
                // Change duration for all selected triggers
                // NOTE: This switches from DYNAMIC to FIXED duration
                bool hadDynamicDuration = false;
                for (NxObject *object : selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    QString currentMessage = trigger->getMessagePatternsStr();
                    
                    int channel = 1, note = 60, velocity = 100, duration = 500;
                    QString velocitySource = QString::number(velocity);
                    bool isDynamicDuration = false;
                    
                    if(currentMessage.contains("midi://")) {
                        QStringList parts = currentMessage.split(" ", Qt::SkipEmptyParts);
                        // Format: "interval, midi://midi_out/note channel note velocity duration"
                        // parts[0]=interval, parts[1]=midi://, parts[2]=channel, parts[3]=note, parts[4]=velocity, parts[5]=duration
                        if(parts.count() >= 6) {
                            channel = parts[2].toInt();
                            note = parts[3].toInt();
                            
                            // Preserve velocity (could be dynamic or fixed)
                            if(parts[4].contains("trigger_")) {
                                velocitySource = parts[4]; // Keep dynamic source
                            } else {
                                velocity = parts[4].toInt();
                                velocitySource = QString::number(velocity);
                            }
                            
                            // Check if duration is dynamic
                            if(parts[5].contains("trigger_")) {
                                duration = 500; // Default for switching from dynamic to fixed
                                isDynamicDuration = true;
                                hadDynamicDuration = true;
                            } else {
                                duration = parts[5].toInt();
                                if(duration <= 0) duration = 500;
                            }
                        }
                    }
                    
                    int oldDuration = duration;
                    duration = qMax(10, duration + durationDelta);  // Minimum 10ms
                    
                    // Use /notef if velocity is dynamic, /note if both are fixed
                    QString cmdType = velocitySource.contains("trigger_") ? "notef" : "note";
                    QString midiCommand = QString("midi://midi_out/%1 %2 %3 %4 %5")
                        .arg(cmdType).arg(channel).arg(note).arg(velocitySource).arg(duration / 1000.0);
                    
                    Application::current->execute(QString("%1 %2 21, %3").arg(COMMAND_MESSAGE).arg(trigger->getId()).arg(midiCommand), ExecuteSourceGui);
                    qDebug("[DURATION] Trigger %d: %s %d -> %d ms (using /%s)", trigger->getId(), isDynamicDuration ? "dynamic" : "fixed", oldDuration, duration, qPrintable(cmdType));
                }
                // Show average duration across all selected triggers
                int avgDuration = 0;
                int count = 0;
                for (NxObject *object : selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    QString msg = trigger->getMessagePatternsStr();
                    if(msg.contains("midi://")) {
                        QStringList parts = msg.split(" ", Qt::SkipEmptyParts);
                        if(parts.count() >= 6) {
                            // parts[5] might be a number or "trigger_duration" string
                            bool ok;
                            int dur = parts[5].toInt(&ok);
                            if(ok) {
                                avgDuration += dur;
                                count++;
                            }
                        }
                    }
                }
                if(count > 0) avgDuration /= count;
                QString statusMsg = QString("Duration: %1%2 ms (now %3 ms)")
                    .arg(durationDelta > 0 ? "+" : "").arg(durationDelta).arg(avgDuration);
                if(hadDynamicDuration) {
                    statusMsg += " [switched to FIXED]";
                }
                changeStatus(statusMsg);
                event->accept();
                return;
            }
            
            if(noteOffset >= 0) {
                // Check if any selected trigger has MIDI message
                bool hasMidiTrigger = false;
                foreach(const NxObject *object, selection) {
                    NxTrigger *trigger = (NxTrigger*)object;
                    if(trigger->getMessagePatternsStr().contains("midi://")) {
                        hasMidiTrigger = true;
                        break;
                    }
                }
                if(!hasMidiTrigger) {
                    changeStatus(QString("No MIDI triggers selected"));
                    event->accept();
                    return;
                }
                
                // Calculate MIDI note: currentOctave maps to MIDI octave (e.g., octave 4 = C4 = MIDI 60)
                // Formula: currentOctave * 12 + noteOffset
                int newMidiNote = currentOctave * 12 + noteOffset;
                if(newMidiNote >= 0 && newMidiNote <= 127) {
                    Application::current->pushSnapshot();
                    
                    // Apply to each trigger individually, preserving its own parameters
                    for (NxObject *object : selection) {
                        NxTrigger *trigger = (NxTrigger*)object;
                        QString currentMessage = trigger->getMessagePatternsStr();
                        
                        int channel = 1;
                        QString velocitySource = "trigger_value_x";  // Dynamic velocity from X position
                        QString durationSource = "trigger_duration"; // Dynamic duration
                        
                        // Parse existing message to preserve channel and check format
                        if(currentMessage.contains("midi://")) {
                            QStringList parts = currentMessage.split(" ", Qt::SkipEmptyParts);
                            // Format: "interval, midi://midi_out/note channel note velocity duration"
                            if(parts.count() >= 6) {
                                channel = parts[2].toInt();
                                // Check if velocity/duration are dynamic (contain trigger_)
                                if(parts[4].contains("trigger_"))
                                    velocitySource = parts[4];
                                if(parts[5].contains("trigger_"))
                                    durationSource = parts[5];
                            }
                        }
                        
                        // Create MIDI command with FIXED note but DYNAMIC velocity (X position)
                        // This preserves "drawn music" philosophy for expression while giving pitch control
                        // Use /notef format with normalized note value (note/127.0)
                        QString midiCommand = QString("midi://midi_out/notef %1 %2 %3 %4")
                            .arg(channel)
                            .arg(newMidiNote / 127.0, 0, 'f', 4)  // Normalize: 0-127 -> 0.0-1.0
                            .arg(velocitySource)     // Dynamic: trigger_value_x (already 0-1)
                            .arg(durationSource);     // Dynamic: trigger_duration
                        
                        Application::current->execute(QString("%1 %2 21, %3")
                            .arg(COMMAND_MESSAGE).arg(trigger->getId()).arg(midiCommand), ExecuteSourceGui);
                    }
                    
                    // Display note name with correct octave
                    // MIDI octave calculation: octaveNumber = (midiNote / 12) - 1
                    // But for display we use: octaveNumber = midiNote / 12
                    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                    int displayOctave = newMidiNote / 12;
                    QString noteName = QString("%1%2").arg(noteNames[newMidiNote % 12]).arg(displayOctave);
                    changeStatus(QString("Note: %1 (MIDI %2) | Velocity: dynamic (X pos)").arg(noteName).arg(newMidiNote));
                    
                    event->accept();
                    return;
                }
            }
        }
    }
    
skipMusicInput:
    // Manual trigger test with Enter key (Space is used for playback)
    // Ctrl+Enter triggers each selected trigger twice.
    if((event->nativeScanCode() == 28 || event->nativeScanCode() == 36) && selection.count() > 0) {  // Enter or Numpad Enter
        const int repeatCount = ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier) ? 2 : 1;
        triggerSelectedTriggers(repeatCount);
        changeStatus(QString("Triggered %1 object(s) x%2").arg(selection.count()).arg(repeatCount));
        event->accept();
        return;
    }

    NxPoint translation;
    if(event->key() == Qt::Key_Left)
        translation += NxPoint(translationUnit, 0);
    else if(event->key() == Qt::Key_Right)
        translation += NxPoint(-translationUnit, 0);
    else if(event->key() == Qt::Key_Up)
        translation += NxPoint(0, -translationUnit);
    else if(event->key() == Qt::Key_Down)
        translation += NxPoint(0, translationUnit);

    if(event->key() == Qt::Key_Escape) {
        emit(escFullscreen());
        emit(editingStop());
    }
    else if(event->key() == Qt::Key_Escape) {
        emit(editingStop());
        Render::editing = false;
    }

    if((selection.count() > 0) && ((translation.x() != 0) || (translation.y() != 0))) {
        Application::current->pushSnapshot();
        Application::current->execute(QString("%1 selection %2 %3 %4").arg(COMMAND_POS_TRANSLATE).arg(-translation.x()).arg(-translation.y()).arg(-translation.z()), ExecuteSourceGui);
        emit(selectionChanged());
    }
    else {
        NxPoint newCenter = -(Render::axisCenterDest + translation);
        if(newCenter != -Render::axisCenterDest)
            Application::current->execute(QString("%1 %2 %3").arg(COMMAND_CENTER).arg(newCenter.x()).arg(newCenter.y()), ExecuteSourceGui);
    }
}
void UiRender::keyReleaseEvent(QKeyEvent *) {
    Application::mouseSnapX = snapBeforeKeyX;
    Application::mouseSnapY = snapBeforeKeyY;
}

bool UiRender::event(QEvent *event) {
    switch (event->type()) {
    case QEvent::Gesture: {
        QGestureEvent *gestureEvent = (QGestureEvent*)event;
        QList<QGesture*> gestures = gestureEvent->activeGestures();
        foreach(const QGesture *gesture, gestures) {
            if(gesture->gestureType() == Qt::PinchGesture) {
                QPinchGesture *pinch = (QPinchGesture*)gesture;
                if(pinch->state() == Qt::GestureStarted)
                    pinchValue = Render::zoomValue;
                setZoom(pinchValue * 1.0F / pinch->scaleFactor());
            }
        }
        return true;
    }
        break;
    default:
        break;
    }
#ifdef USE_GLWIDGET
    return QGLWidget::event(event);
#else
    return QOpenGLWidget::event(event);
#endif
}
void UiRender::dragEnterEvent(QDragEnterEvent *event) {
    const QMimeData* mimeData = event->mimeData();
    bool ok = false;
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for(int i = 0; i < urlList.size() && i < 32; ++i) {
            QString filename = urlList.at(i).toLocalFile();
            if(filename.toLower().endsWith("svg"))
                ok = true;
            else if((filename.toLower().endsWith("png")) || (filename.toLower().endsWith("jpg")) || (filename.toLower().endsWith("jpeg")))
                ok = true;
        }
    }
    else if(!mimeData->text().isEmpty())
        ok = true;
    if(ok)
        event->acceptProposedAction();
}
void UiRender::dropEvent(QDropEvent *event) {
    const QMimeData* mimeData = event->mimeData();
    bool ok = false;
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for(int i = 0; i < urlList.size() && i < 32; ++i) {
            QString filename = urlList.at(i).toLocalFile();
            if(filename.toLower().endsWith("svg")) {
                ok = true;
                actionImportSVG(filename);
            }
            else if((filename.toLower().endsWith("png")) || (filename.toLower().endsWith("jpg")) || (filename.toLower().endsWith("jpeg"))) {
                actionImportBackground(filename);
            }
        }
    }
    else if(!mimeData->text().isEmpty()) {
        ok = true;
        actionImportText("", mimeData->text());
    }
    if(ok)
        event->acceptProposedAction();
}

void UiRender::selectionClear(bool hoverAlso) {
    //Clear selection
    for (NxObject *selected : selection)
        selected->setSelected(false);
    for (NxObject *selected : selectionRect)
        selected->setSelected(false);
    selection.clear();
    if(hoverAlso) {
        if(selectedHover)
            selectedHover->setSelectedHover(false);
        selectedHover = 0;
    }
    emit(selectionChanged());
}
void UiRender::selectionAdd(NxObject *object) {
    if(object) {
        object->setSelected(true);
        if(!selection.contains(object))
            selection.append(object);
        emit(selectionChanged());
    }
}

void UiRender::setZoom() {
    Render::zoomLinearDest = qMax((qreal)0.01, Render::zoomValue / 100.F);
    emit(mouseZoomChanged(100.0F / Render::zoomLinearDest));
    Render::zoomLinearDest *= 1.3;
}
void UiRender::setZoom(qreal axisZoom) {
    Render::zoomValue = qMax((qreal)0, axisZoom);
    setZoom();
}
void UiRender::zoomIn() {
    setZoom(Render::zoomValue - 5);
}
void UiRender::zoomOut() {
    setZoom(Render::zoomValue + 5);
}
void UiRender::zoomInitial() {
    setZoom(100);
}




void UiRender::actionDuplicate() {
    Application::current->pushSnapshot();
    emit(actionRouteCopy());
    emit(actionRoutePaste());
}

void UiRender::actionCut() {
    Application::current->pushSnapshot();
    emit(actionRouteCopy());
    QStringList commands;
    for (NxObject *object : selection)
        commands << QString(COMMAND_REMOVE) + " " + QString::number(object->getId()) + COMMAND_END;
    for (const QString & command : commands)
        Application::current->execute(command, ExecuteSourceCopyPaste);
}

void UiRender::actionSelect_all() {
    if(documentToRender) {
        selectionClear();

        //Browse documents
        for (NxGroup *group : documentToRender->groups) {
            //Is groups visible ?
            if((((!Application::current->isGroupSoloActive) && (group->isNotMuted())) || ((Application::current->isGroupSoloActive) && (group->isSolo())))) {

                //Browse active/inactive objects
                for(quint16 activityIterator = 0 ; activityIterator < ObjectsActivityLenght ; activityIterator++) {

                    //Browse all types of objects
                    for(quint16 typeIterator = 0 ; typeIterator < ObjectsTypeLength ; typeIterator++) {

                        //Are objects visible ?
                        if(((typeIterator == ObjectsTypeCursor) && (Application::allowSelectionCursors)) || ((typeIterator == ObjectsTypeCurve) && (Application::allowSelectionCurves)) || ((typeIterator == ObjectsTypeTrigger) && (Application::allowSelectionTriggers))) {
                            //Browse objects
                            for (NxObject *object : group->objects[activityIterator][typeIterator]) {
                                //Is Z visible ?
                                if(((!Application::current->isObjectSoloActive) && (object->isNotMuted())) || ((Application::current->isObjectSoloActive) && (object->isSolo()))) {

                                    //Add the object to selection
                                    selectionAdd(object);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
void UiRender::actionDelete() {
    Application::current->pushSnapshot();
    QStringList commands;
    if(selection.count() == Application::current->getCount()) {
        Application::current->execute(COMMAND_CLEAR, ExecuteSourceGui);
        selectionClear();
        selectedHover = 0;
    }
    else {
        for (NxObject *object : selection)
            commands << QString(COMMAND_REMOVE) + " " + QString::number(object->getId());
        selectionClear();
        selectedHover = 0;
        for (const QString & command : commands)
            Application::current->execute(command, ExecuteSourceGui);
    }
}

void UiRender::arrangeObjects(NxObject *objet, const NxPoint &pt) {
    Application::current->execute(QString("%1 %2 %3 %4 %5").arg(COMMAND_POS).arg(objet->getId()).arg(pt.x()).arg(pt.y()).arg(pt.z()), ExecuteSourceGui);
}
void UiRender::arrangeObjects(quint16 type) {
    qreal top = -99999999, left = 99999999, bottom = 99999999, right = -99999999;
    qreal dtop = -99999999, dleft = 99999999, dbottom = 99999999, dright = -99999999;
    qreal nbObj = 0;

    Application::current->pushSnapshot();

    //Browse objects
    foreach(const NxObject *object, selection) {
        if(object->getType() == ObjectsTypeCurve) {
            top    = qMax(top,    qMax(object->getBoundingRect().top(),  object->getBoundingRect().bottom()));
            left   = qMin(left,   qMin(object->getBoundingRect().left(), object->getBoundingRect().right()));
            bottom = qMin(bottom, qMin(object->getBoundingRect().top(),  object->getBoundingRect().bottom()));
            right  = qMax(right,  qMax(object->getBoundingRect().left(), object->getBoundingRect().right()));
            dtop    = qMax(dtop,    object->getBoundingRect().center().y());
            dleft   = qMin(dleft,   object->getBoundingRect().center().x());
            dbottom = qMin(dbottom, object->getBoundingRect().center().y());
            dright  = qMax(dright,  object->getBoundingRect().center().x());
            nbObj++;
        }
        else if(object->getType() == ObjectsTypeTrigger) {
            top    = qMax(top,    object->getPos().y());
            left   = qMin(left,   object->getPos().x());
            bottom = qMin(bottom, object->getPos().y());
            right  = qMax(right,  object->getPos().x());
            dtop    = qMax(dtop,    object->getPos().y());
            dleft   = qMin(dleft,   object->getPos().x());
            dbottom = qMin(dbottom, object->getPos().y());
            dright  = qMax(dright,  object->getPos().x());
            nbObj++;
        }
    }
    qreal width = (right-left)/2;
    qreal center = left + width;
    qreal height = (top-bottom)/2;
    qreal middle = bottom + height;

    if(type == 0) {
        //Top
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve)
                arrangeObjects(object, NxPoint(object->getPos().x(), object->getPos().y() + top-qMax(object->getBoundingRect().top(), object->getBoundingRect().bottom()), object->getPos().z()));
            else if(object->getType() == ObjectsTypeTrigger)
                arrangeObjects(object, NxPoint(object->getPos().x(), top, object->getPos().z()));
    }
    else if(type == 1) {
        //Left
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve)
                arrangeObjects(object, NxPoint(object->getPos().x() + left-qMin(object->getBoundingRect().left(), object->getBoundingRect().right()), object->getPos().y(), object->getPos().z()));
            else if(object->getType() == ObjectsTypeTrigger)
                arrangeObjects(object, NxPoint(left, object->getPos().y(), object->getPos().z()));
    }
    else if(type == 2) {
        //Bottom
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve)
                arrangeObjects(object, NxPoint(object->getPos().x(), object->getPos().y() + bottom-qMin(object->getBoundingRect().top(), object->getBoundingRect().bottom()), object->getPos().z()));
            else if(object->getType() == ObjectsTypeTrigger)
                arrangeObjects(object, NxPoint(object->getPos().x(), bottom, object->getPos().z()));
    }
    else if(type == 3) {
        //Right
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve)
                arrangeObjects(object, NxPoint(object->getPos().x() + right-qMax(object->getBoundingRect().left(), object->getBoundingRect().right()), object->getPos().y(), object->getPos().z()));
            else if(object->getType() == ObjectsTypeTrigger)
                arrangeObjects(object, NxPoint(right, object->getPos().y(), object->getPos().z()));
    }
    else if(type == 4) {
        //Middle
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve)
                arrangeObjects(object, NxPoint(object->getPos().x(), object->getPos().y() + middle-object->getBoundingRect().center().y(), object->getPos().z()));
            else if(object->getType() == ObjectsTypeTrigger)
                arrangeObjects(object, NxPoint(object->getPos().x(), middle, object->getPos().z()));
    }
    else if(type == 5) {
        //Center
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve)
                arrangeObjects(object, NxPoint(object->getPos().x() + middle-object->getBoundingRect().center().x(), object->getPos().y(), object->getPos().z()));
            else if(object->getType() == ObjectsTypeTrigger)
                arrangeObjects(object, NxPoint(center, object->getPos().y(), object->getPos().z()));
    }
    else if((type == 6) && (nbObj > 1)) {
        //Distrib H
        qreal dHStep = qAbs(dright-dleft) / (nbObj-1);
        nbObj = 0;
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve) {
                arrangeObjects(object, NxPoint(object->getPos().x() + (dleft + nbObj * dHStep)-object->getBoundingRect().center().x(), object->getPos().y(), object->getPos().z()));
                nbObj++;
            }
            else if(object->getType() == ObjectsTypeTrigger) {
                arrangeObjects(object, NxPoint(dleft + nbObj * dHStep, object->getPos().y(), object->getPos().z()));
                nbObj++;
            }
    }
    else if((type == 7) && (nbObj > 1)) {
        //Distrib V
        qreal dVStep = qAbs(dtop-dbottom) / (nbObj-1);
        nbObj = 0;
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve) {
                arrangeObjects(object, NxPoint(object->getPos().x(), object->getPos().y() + (dbottom + nbObj * dVStep)-object->getBoundingRect().center().y(), object->getPos().z()));
                nbObj++;
            }
            else if(object->getType() == ObjectsTypeTrigger) {
                arrangeObjects(object, NxPoint(object->getPos().x(), dbottom + nbObj * dVStep, object->getPos().z()));
                nbObj++;
            }
    }
    else if(type == 8) {
        //Distrib Circle
        qreal dAngle = 2 * M_PI / nbObj;
        nbObj = 0;
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve) {
                arrangeObjects(object, NxPoint(object->getPos().x() + (center + qMax(width,height)*qCos(dAngle * nbObj))-object->getBoundingRect().center().x(), object->getPos().y() + (middle + qMax(width,height)*qSin(dAngle * nbObj))-object->getBoundingRect().center().y(), object->getPos().z()));
                nbObj++;
            }
            else if(object->getType() == ObjectsTypeTrigger) {
                arrangeObjects(object, NxPoint(center + qMax(width,height)*qCos(dAngle * nbObj), middle + qMax(width,height)*qSin(dAngle * nbObj), object->getPos().z()));
                nbObj++;
            }
    }
    else if(type == 9) {
        //Distrib Ellipse
        qreal dAngle = 2 * M_PI / nbObj;
        nbObj = 0;
        for (NxObject *object : selection)
            if(object->getType() == ObjectsTypeCurve) {
                arrangeObjects(object, NxPoint(object->getPos().x() + (center + width*qCos(dAngle * nbObj))-object->getBoundingRect().center().x(), object->getPos().y() + (middle + height*qSin(dAngle * nbObj))-object->getBoundingRect().center().y(), object->getPos().z()));
                nbObj++;
            }
            else if(object->getType() == ObjectsTypeTrigger) {
                arrangeObjects(object, NxPoint(center + width*qCos(dAngle * nbObj), middle + height*qSin(dAngle * nbObj), object->getPos().z()));
                nbObj++;
            }
    }

}


#ifdef USE_OPENGLWIDGET
void UiRender::renderText(qreal x, qreal y, qreal z, const QString &text, const QFont &, bool billboarded) {
    while(OpenGlTexture::textures.count() < 500) {
        OpenGlTexture *texte = new OpenGlTexture(this, text, renderTextFont, QSizeF(1024, 128) * OpenGlDrawing::dpi);
        OpenGlTexture::textures.append(texte);
    }
    OpenGlTexture *textTextureToUse = 0;
    for (OpenGlTexture *textTexture : OpenGlTexture::textures)
        if(textTexture->texte == text) {
            textTextureToUse = textTexture;
            break;
        }

    if(!textTextureToUse) {
        OpenGlTexture::textures[renderTextTextureIndex]->loadTexte(text, renderTextFont, OpenGlTexture::textures.at(renderTextTextureIndex)->size);
        textTextureToUse = OpenGlTexture::textures.at(renderTextTextureIndex);
        //qDebug("ICI GENERATION %d", renderTextTextureIndex);
        renderTextTextureIndex = (renderTextTextureIndex+1) % OpenGlTexture::textures.count();
    }
    if(textTextureToUse) {
        glPushMatrix();
        glTranslatef(x, y, z);

        //Texte billboardé ou non
        if(billboarded) {
            glRotatef(Render::rotation.z(), 0, 0, -1);
            glRotatef(Render::rotation.x(), 0, -1, 0);
            glRotatef(Render::rotation.y(), -1, 0, 0);
        }

        //glScalef(0.05/(scale*OpenGlDrawing::dpi), -0.05/(scale*OpenGlDrawing::dpi), 0.05/(scale*OpenGlDrawing::dpi));
        qreal textScale = getAutoScale(1) * 0.06;
        glScalef(textScale, -textScale, textScale);
        textTextureToUse->pushTexture();
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(0, textTextureToUse->size.height(), 0);
        glTexCoord2f(1, 0); glVertex3f(textTextureToUse->size.width(), textTextureToUse->size.height(), 0);
        glTexCoord2f(1, 1); glVertex3f(textTextureToUse->size.width(), 0, 0);
        glTexCoord2f(0, 1); glVertex3f(0, 0, 0);
        glEnd();
        textTextureToUse->popTexture();
        glPopMatrix();
    }
}
#endif
