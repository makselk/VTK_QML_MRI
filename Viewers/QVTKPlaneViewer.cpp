#include "QVTKPlaneViewer.h"
#include "MriDataProvider.h"

#include <vtkNew.h>
#include <vtkCommand.h>
#include <vtkInteractorStyleImage.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageActor.h>
#include <vtkPointData.h>


QVTKPlaneViewerItem::QVTKPlaneViewerItem() {
    this->setMirrorVertically(true);
    this->setAcceptedMouseButtons(Qt::RightButton | Qt::LeftButton | Qt::MiddleButton);
}

QQuickFramebufferObject::Renderer* QVTKPlaneViewerItem::createRenderer() const {
    return new QVTKPlaneViewerRenderer();
}

bool QVTKPlaneViewerItem::isInitialized() const {
    return (m_fbo_renderer != nullptr);
}

void QVTKPlaneViewerItem::setFboRenderer(QVTKPlaneViewerRenderer *renderer) {
    m_fbo_renderer = renderer;
}

void QVTKPlaneViewerItem::mouseMoveEvent(QMouseEvent *e) {
    m_fbo_renderer->setMouseMoveEvent(e);
    e->accept();
    this->update();
}

void QVTKPlaneViewerItem::mousePressEvent(QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
        m_fbo_renderer->setMousePressEventL(e);

    else if(e->button() == Qt::RightButton)
        m_fbo_renderer->setMousePressEventR(e);

    else if(e->button() == Qt::MiddleButton)
        m_fbo_renderer->setMousePressEventW(e);

    e->accept();
    this->update();
}

void QVTKPlaneViewerItem::mouseReleaseEvent(QMouseEvent *e) {
    if(e->button() == Qt::LeftButton)
        m_fbo_renderer->setMouseReleaseEventL(e);

    else if (e->button() == Qt::RightButton)
        m_fbo_renderer->setMouseReleaseEventR(e);

    else if(e->button() == Qt::MiddleButton)
        m_fbo_renderer->setMouseReleaseEventW(e);

    e->accept();
    this->update();
}

int QVTKPlaneViewerItem::getOrientation() const {
    return orientation;
}

void QVTKPlaneViewerItem::setOrientation(const int& orientation) {
    this->orientation = orientation;
}

QVTKPlaneViewerRenderer* QVTKPlaneViewerItem::getRenderer() {
    return m_fbo_renderer;
}




QVTKPlaneViewerRenderer::QVTKPlaneViewerRenderer() {
    // Инициализация функций OpenGL
    QOpenGLFunctions::initializeOpenGLFunctions();
    QOpenGLFunctions::glUseProgram(0);

    // Инициализация полей рендеринга
    m_render_window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_interactor = vtkSmartPointer<vtkGenericRenderWindowInteractor>::New();
    m_renderer = vtkSmartPointer<vtkRenderer>::New();

    // Обычное задание background'а не работает, только через градиент
    m_renderer->SetBackground(0,0,0);
    m_renderer->SetBackground2(0,0,0);
    m_renderer->GradientBackgroundOn();

    // Задаем стиль взаимодействия с объектом
    vtkNew<vtkInteractorStyleImage> styl;
    m_interactor->SetInteractorStyle(styl);

    m_render_window->AddRenderer(m_renderer);
    m_render_window->SetInteractor(m_interactor);

    // Инициализация OpenGL контекста под окно vtk
    m_render_window->OpenGLInitContext();
    this->update();
}

void QVTKPlaneViewerRenderer::synchronize(QQuickFramebufferObject * item) {
    // При первом вызове инициализирует указаель на
    // qml объект и дает ему ссылку на себя
    if(!m_item) {
        m_item = static_cast<QVTKPlaneViewerItem*>(item);
        m_item->setFboRenderer(this);
        MriDataProvider::getInstance().addPlaneViewer(m_item);
    }
}

// Called from the render thread when the GUI thread is NOT blocked
void QVTKPlaneViewerRenderer::render() {
    // Иницализация параметров для публикации
    m_render_window->OpenGLInitState();
    // Делаем поток текщим
    m_render_window->MakeCurrent();
    // Инициализация сцены при первом рендеринге
    if(data_changed)
        this->updateData();
    // Изменение текщуего среза
    if(slice_changed) {
        m_image_viewer->SetSlice(m_slice);
        slice_changed = false;
    }
    if(window_changed) {
        m_image_viewer->SetColorWindow(m_window);
        window_changed = false;
    }
    if(level_changed) {
        m_image_viewer->SetColorLevel(m_level);
        level_changed = false;
    }
    // Обработка событий
    if(interaction)
        this->handleEvents();
    // Публикация
    m_render_window->Render();
}

void QVTKPlaneViewerRenderer::setData(vtkImageData* data) {
    // Вытаскиваем обновленные данные
    this->data = data;

    // Сохраняем размеры предыдущего окна
    int* size = m_render_window->GetSize();

    // Инициализируем окна заново
    m_render_window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_interactor = vtkSmartPointer<vtkGenericRenderWindowInteractor>::New();

    // Инициализация объекта для просмотра плоских изображений
    m_image_viewer = vtkSmartPointer<vtkResliceImageViewer>::New();
    // Если использовать без даункста, иногда почему-то падает
    vtkSmartPointer<vtkRenderWindow> ren_win = vtkRenderWindow::SafeDownCast(m_render_window);
    m_image_viewer->SetRenderWindow(ren_win);
    m_image_viewer->SetupInteractor(m_interactor);
    m_renderer = m_image_viewer->GetRenderer();

    // Возвращаем новому окну нужные параметры
    m_render_window->SetSize(size[0], size[1]);
    m_render_window->SetOffScreenRendering(true);
    m_render_window->SetUseOffScreenBuffers(true);
    m_render_window->Modified();

    interaction = false;
    data_changed = true;

    this->update();
}

void QVTKPlaneViewerRenderer::setSlice(int slice) {
    if(!m_image_viewer)
        return;
    m_slice = slice;
    slice_changed = true;
    this->update();
}

void QVTKPlaneViewerRenderer::setWindow(int value) {
    if(!m_image_viewer)
        return;
    m_window = value;
    window_changed = true;
    this->update();
}

void QVTKPlaneViewerRenderer::setLevel(int value) {
    if(!m_image_viewer)
        return;
    m_level = value;
    level_changed = true;
    this->update();
}

QOpenGLFramebufferObject* QVTKPlaneViewerRenderer::createFramebufferObject(const QSize &size) {
    // Создаем OpenGLFrameBufferObject
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::Depth);
    std::unique_ptr<QOpenGLFramebufferObject> framebufferObject(new QOpenGLFramebufferObject(size, format));

    // Задаем размер, равный буфферу
    m_render_window->SetSize(framebufferObject->size().width(), framebufferObject->size().height());
    // Разрешаем рендеринг вне окна
    m_render_window->SetOffScreenRendering(true);
    m_render_window->SetUseOffScreenBuffers(true);
    m_render_window->Modified();

    return framebufferObject.release();
}

void QVTKPlaneViewerRenderer::setMouseMoveEvent(QMouseEvent* e) {
    m_mouse_move = std::make_shared<QMouseEvent>(*e);
    m_mouse_move->ignore();
}

void QVTKPlaneViewerRenderer::setMousePressEventW(QMouseEvent* e) {
    m_mouse_press_w = std::make_shared<QMouseEvent>(*e);
    m_mouse_press_w->ignore();
}

void QVTKPlaneViewerRenderer::setMousePressEventL(QMouseEvent* e) {
    m_mouse_press_l = std::make_shared<QMouseEvent>(*e);
    m_mouse_press_l->ignore();
}

void QVTKPlaneViewerRenderer::setMousePressEventR(QMouseEvent* e) {
    m_mouse_press_r = std::make_shared<QMouseEvent>(*e);
    m_mouse_press_r->ignore();
}

void QVTKPlaneViewerRenderer::setMouseReleaseEventW(QMouseEvent* e) {
    m_mouse_release_w = std::make_shared<QMouseEvent>(*e);
    m_mouse_release_w->ignore();
}

void QVTKPlaneViewerRenderer::setMouseReleaseEventL(QMouseEvent* e) {
    m_mouse_release_l = std::make_shared<QMouseEvent>(*e);
    m_mouse_release_l->ignore();
}

void QVTKPlaneViewerRenderer::setMouseReleaseEventR(QMouseEvent* e) {
    m_mouse_release_r = std::make_shared<QMouseEvent>(*e);
    m_mouse_release_r->ignore();
}

void QVTKPlaneViewerRenderer::handleEvents() {
    // Колесо мыши нажато
    //if(m_mouse_press_w && !m_mouse_press_w->isAccepted()) {
    //    m_interactor->SetEventInformationFlipY(m_mouse_press_w->x(), m_mouse_press_w->y());
    //    m_interactor->InvokeEvent(vtkCommand::MiddleButtonPressEvent, m_mouse_press_w.get());
    //    m_mouse_press_w->accept();
    //}

    // ЛКМ нажата
    if(m_mouse_press_l && !m_mouse_press_l->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_press_l->x(), m_mouse_press_l->y());
        //m_interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent, m_mouse_press_l.get());
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
    //if(m_mouse_release_w && !m_mouse_release_w->isAccepted()) {
    //    m_interactor->SetEventInformationFlipY(m_mouse_release_w->x(), m_mouse_release_w->y());
    //    m_interactor->InvokeEvent(vtkCommand::MiddleButtonReleaseEvent, m_mouse_release_w.get());
    //    m_mouse_release_w->accept();
    //}

    // ЛКМ отжата
    if(m_mouse_release_l && !m_mouse_release_l->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_release_l->x(), m_mouse_release_l->y());
        //m_interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent, m_mouse_release_l.get());
        m_mouse_release_l->accept();
    }

    // ПКМ отжата
    if(m_mouse_release_r && !m_mouse_release_r->isAccepted()) {
        m_interactor->SetEventInformationFlipY(m_mouse_release_r->x(), m_mouse_release_r->y());
        m_interactor->InvokeEvent(vtkCommand::RightButtonReleaseEvent, m_mouse_release_r.get());
        m_mouse_release_r->accept();
    }
}


void QVTKPlaneViewerRenderer::updateData() {
    m_renderer->SetBackground(0,0,0);
    m_renderer->SetBackground2(0,0,0);
    m_renderer->GradientBackgroundOn();

    m_image_viewer->SetInputData(data);
    m_image_viewer->SetSliceOrientation(m_item->orientation);
    m_image_viewer->SetResliceModeToAxisAligned();
    m_image_viewer->SetSlice(data->GetDimensions()[m_item->orientation] / 2);
    m_image_viewer->GetImageActor()->SetForceOpaque(true);

    interaction = true;
    data_changed = false;
}
