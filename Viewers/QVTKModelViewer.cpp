#include "MriDataProvider.h"
#include "QVTKModelViewer.h"

#include <vtkNew.h>
#include <vtkCommand.h>
#include <vtkProperty.h>
#include <vtkCellPicker.h>
#include <vtkInteractorStyleTrackballCamera.h>


QVTKModelViewerItem::QVTKModelViewerItem() {
    this->setMirrorVertically(true);
    this->setAcceptedMouseButtons(Qt::RightButton | Qt::LeftButton | Qt::MiddleButton);
}

QQuickFramebufferObject::Renderer* QVTKModelViewerItem::createRenderer() const {
    return new QVTKModelViewerRenderer();
}

bool QVTKModelViewerItem::isInitialized() const {
    return (m_fbo_renderer != nullptr);
}

void QVTKModelViewerItem::setFboRenderer(QVTKModelViewerRenderer *renderer) {
    m_fbo_renderer = renderer;
}

QVTKModelViewerRenderer* QVTKModelViewerItem::getRenderer() {
    return m_fbo_renderer;
}

void QVTKModelViewerItem::wheelEvent(QWheelEvent *e) {
    m_fbo_renderer->setWheelEvent(e);
    e->accept();
    this->update();
}

void QVTKModelViewerItem::mouseMoveEvent(QMouseEvent *e) {
    m_fbo_renderer->setMouseMoveEvent(e);
    e->accept();
    this->update();
}

void QVTKModelViewerItem::mousePressEvent(QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
        m_fbo_renderer->setMousePressEventL(e);

    else if(e->button() == Qt::RightButton)
        m_fbo_renderer->setMousePressEventR(e);

    else if(e->button() == Qt::MiddleButton)
        m_fbo_renderer->setMousePressEventW(e);

    e->accept();
    this->update();
}

void QVTKModelViewerItem::mouseReleaseEvent(QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
        m_fbo_renderer->setMouseReleaseEventL(e);

    else if (e->button() == Qt::RightButton)
        m_fbo_renderer->setMouseReleaseEventR(e);

    else if(e->button() == Qt::MiddleButton)
        m_fbo_renderer->setMouseReleaseEventW(e);

    e->accept();
    this->update();
}




QVTKModelViewerRenderer::QVTKModelViewerRenderer() {
    // Инициализация функций OpenGL
    QOpenGLFunctions::initializeOpenGLFunctions();
    QOpenGLFunctions::glUseProgram(0);

    // Renderer
    m_render_window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();
    m_render_window->AddRenderer(m_renderer);

    // Interactor
    m_interactor = vtkSmartPointer<vtkGenericRenderWindowInteractor>::New();
    vtkNew<vtkInteractorStyleTrackballCamera> styl;
    m_interactor->SetInteractorStyle(styl);
    m_interactor->EnableRenderOff();
    m_render_window->SetInteractor(m_interactor);

    m_renderer->SetBackground(0.5, 0.5, 0.7);
    m_renderer->SetBackground2(0.7, 0.5, 0.5);
    m_renderer->GradientBackgroundOn();

    // Initialize the OpenGL context for the renderer
    m_render_window->OpenGLInitContext();

    this->update();
}

void QVTKModelViewerRenderer::synchronize(QQuickFramebufferObject * item) {
    // При первом вызове инициализирует указаель на
    // qml объект и дает ему ссылку на себя
    if(!m_item) {
        m_item = static_cast<QVTKModelViewerItem*>(item);
        m_item->setFboRenderer(this);
    }
    MriDataProvider::getInstance().addModelViewer(m_item);
}

// Called from the render thread when the GUI thread is NOT blocked
void QVTKModelViewerRenderer::render() {
    // Иницализация параметров для публикации
    m_render_window->OpenGLInitState();
    // Делаем поток текщим
    m_render_window->MakeCurrent();

    if(data_changed)
        this->initPlaneViewers();

    if(window_level_changed)
        this->updateWindowLevel();

    if(slice_changed)
        this->updateSlice();

    // Обработка событий
    this->handleEvents();

    // Публикация
    m_render_window->Render();
    // Возвращаются исходные параметры
    m_item->window()->resetOpenGLState();
}

void QVTKModelViewerRenderer::addActor(vtkActor *actor) {
    m_renderer->AddActor(actor);
    this->update();
}

void QVTKModelViewerRenderer::removeActor(vtkActor *actor) {
    m_renderer->RemoveActor(actor);
    this->update();
}

void QVTKModelViewerRenderer::setData(vtkImageData* data) {
    this->data = data;
    data_changed = true;
    this->update();
}

void QVTKModelViewerRenderer::setSlice(int i, int slice) {
    if(!plane_widget[0])
        return;
    m_slice[i] = slice;
    slice_changed = true;
    this->update();
}

void QVTKModelViewerRenderer::setWindow(int window) {
    if(!plane_widget[0])
        return;
    m_window = window;
    window_level_changed = true;
    this->update();
}

void QVTKModelViewerRenderer::setLevel(int level) {
    if(!plane_widget[0])
        return;
    m_level = level;
    window_level_changed = true;
    this->update();
}

QOpenGLFramebufferObject* QVTKModelViewerRenderer::createFramebufferObject(const QSize &size) {
    // Создаем OpenGLFrameBufferObject
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    std::unique_ptr<QOpenGLFramebufferObject> framebufferObject(new QOpenGLFramebufferObject(size, format));

    // Задаем размер, равный буфферу
    m_render_window->SetSize(framebufferObject->size().width(), framebufferObject->size().height());
    // Разрешаем рендеринг вне окна
    m_render_window->SetOffScreenRendering(true);
    m_render_window->Modified();

    return framebufferObject.release();
}

void QVTKModelViewerRenderer::setWheelEvent(QWheelEvent* e) {
    m_mouse_wheel = std::make_shared<QWheelEvent>(*e);
    m_mouse_wheel->ignore();
}

void QVTKModelViewerRenderer::setMouseMoveEvent(QMouseEvent* e) {
    m_mouse_move = std::make_shared<QMouseEvent>(*e);
    m_mouse_move->ignore();
}

void QVTKModelViewerRenderer::setMousePressEventW(QMouseEvent* e) {
    m_mouse_press_w = std::make_shared<QMouseEvent>(*e);
    m_mouse_press_w->ignore();
}

void QVTKModelViewerRenderer::setMousePressEventL(QMouseEvent* e) {
    m_mouse_press_l = std::make_shared<QMouseEvent>(*e);
    m_mouse_press_l->ignore();
}

void QVTKModelViewerRenderer::setMousePressEventR(QMouseEvent* e) {
    m_mouse_press_r = std::make_shared<QMouseEvent>(*e);
    m_mouse_press_r->ignore();
}

void QVTKModelViewerRenderer::setMouseReleaseEventW(QMouseEvent* e) {
    m_mouse_release_w = std::make_shared<QMouseEvent>(*e);
    m_mouse_release_w->ignore();
}

void QVTKModelViewerRenderer::setMouseReleaseEventL(QMouseEvent* e) {
    m_mouse_release_l = std::make_shared<QMouseEvent>(*e);
    m_mouse_release_l->ignore();
}

void QVTKModelViewerRenderer::setMouseReleaseEventR(QMouseEvent* e) {
    m_mouse_release_r = std::make_shared<QMouseEvent>(*e);
    m_mouse_release_r->ignore();
}

void QVTKModelViewerRenderer::handleEvents() {
    // Колесо мыши прокурчено
    if(m_mouse_wheel && !m_mouse_wheel->isAccepted()) {
        if(m_mouse_wheel->angleDelta().y() > 0)
            m_interactor->InvokeEvent(vtkCommand::MouseWheelForwardEvent);
        else
            m_interactor->InvokeEvent(vtkCommand::MouseWheelBackwardEvent);
        m_mouse_wheel->accept();
    }

    // Колесо мыши нажато
    if(m_mouse_press_w && !m_mouse_press_w->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_press_w->x(), m_mouse_press_w->y());
        m_interactor->InvokeEvent(vtkCommand::MiddleButtonPressEvent, m_mouse_press_w.get());
        m_mouse_press_w->accept();
    }

    // ЛКМ нажата
    if(m_mouse_press_l && !m_mouse_press_l->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_press_l->x(), m_mouse_press_l->y());
        m_interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent, m_mouse_press_l.get());
        m_mouse_press_l->accept();
    }

    // ПКМ нажата
    if(m_mouse_press_r && !m_mouse_press_r->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_press_r->x(), m_mouse_press_r->y());
        m_interactor->InvokeEvent(vtkCommand::RightButtonPressEvent, m_mouse_press_r.get());
        m_mouse_press_r->accept();
    }

    // Движение мышью
    if(m_mouse_move && !m_mouse_move->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_move->x(), m_mouse_move->y());
        m_interactor->InvokeEvent(vtkCommand::MouseMoveEvent, m_mouse_move.get());
        m_mouse_move->accept();
    }

    // Колесо мыши отжато
    if(m_mouse_release_w && !m_mouse_release_w->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_release_w->x(), m_mouse_release_w->y());
        m_interactor->InvokeEvent(vtkCommand::MiddleButtonReleaseEvent, m_mouse_release_w.get());
        m_mouse_release_w->accept();
    }

    // ЛКМ отжата
    if(m_mouse_release_l && !m_mouse_release_l->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_release_l->x(), m_mouse_release_l->y());
        m_interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent, m_mouse_release_l.get());
        m_mouse_release_l->accept();
    }

    // ПКМ отжата
    if(m_mouse_release_r && !m_mouse_release_r->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_release_r->x(), m_mouse_release_r->y());
        m_interactor->InvokeEvent(vtkCommand::RightButtonReleaseEvent, m_mouse_release_r.get());
        m_mouse_release_r->accept();
    }
}

void QVTKModelViewerRenderer::initPlaneViewers() {
    data_changed = false;


    if(plane_widget[0]) {
        for(int i = 0; i != 3; ++i) {
            m_slice[i] = data->GetDimensions()[i] / 2;
            plane_widget[i]->SetInputData(data);
            plane_widget[i]->SetSliceIndex(m_slice[i]);
        }
        return;
    }

    for(int i = 0; i != 3; ++i) {
        m_slice[i] = data->GetDimensions()[i] / 2;
        plane_widget[i] = vtkSmartPointer<vtkImagePlaneWidget>::New();
        plane_widget[i]->SetInteractor(m_interactor);
        plane_widget[i]->RestrictPlaneToVolumeOn();
        double color[3] = {0,0,0};
        color[i] = 1;
        plane_widget[i]->GetPlaneProperty()->SetColor(color);

        plane_widget[i]->TextureInterpolateOff();
        plane_widget[i]->SetResliceInterpolateToLinear();
        plane_widget[i]->SetInputData(data);
        plane_widget[i]->SetPlaneOrientation(i);
        plane_widget[i]->SetSliceIndex(m_slice[i]);
        plane_widget[i]->DisplayTextOn();
        plane_widget[i]->SetDefaultRenderer(m_renderer);
        plane_widget[i]->SetWindowLevel(900, 200);
        plane_widget[i]->On();
        plane_widget[i]->InteractionOn();
    }
}

void QVTKModelViewerRenderer::updateWindowLevel() {
    for(int i = 0; i != 3; ++i)
        plane_widget[i]->SetWindowLevel(m_window, m_level);
    window_level_changed = false;
}

void QVTKModelViewerRenderer::updateSlice() {
    for(int i = 0; i != 3; ++i)
        plane_widget[i]->SetSliceIndex(m_slice[i]);
    slice_changed = false;
}
