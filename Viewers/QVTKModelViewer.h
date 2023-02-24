#ifndef QVTK_MODEL_VIEWER_H
#define QVTK_MODEL_VIEWER_H


#include <QtQuick/QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QQuickFramebufferObject>
#include <QOpenGLFramebufferObject>

#include <vtkGenericRenderWindowInteractor.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImagePlaneWidget.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>



class QVTKModelViewerRenderer;


// Непосредственно сам объект, который видит QML
class QVTKModelViewerItem: public QQuickFramebufferObject {
    Q_OBJECT
public:

public:
    QVTKModelViewerItem();
    ~QVTKModelViewerItem() = default;

    // После создания объекта, qml просит создать рендерер,
    // который будет предоставлять OpenGL инофрмацию
    QQuickFramebufferObject::Renderer *createRenderer() const override;

    // Проверка наличия инициализированного поля рендерера
    bool isInitialized() const;
    // Инициализирует поле рендерера
    void setFboRenderer(QVTKModelViewerRenderer* renderer);

    QVTKModelViewerRenderer* getRenderer();

public:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

protected:
    QVTKModelViewerRenderer* m_fbo_renderer = nullptr;
};



// Объект, в котором происходит вся магия передачи изображения
// от VTK к QML через OpenGLFrameBufferObject
class QVTKModelViewerRenderer :
        public QObject,
        public QQuickFramebufferObject::Renderer,
        protected QOpenGLFunctions {

public:
    QVTKModelViewerRenderer();
    ~QVTKModelViewerRenderer() = default;

    // После инициализации рендерера вызывается функция синхронизации
    // для инициализации оставшихся данных (из-за того, что
    // QVTKFrameBufferObjectItem::createRenderer - const, внутри него
    // нельзя определить нестатичные поля)
    // Также вызывается для синхронизации данных рендерера и item'a
    // в случае изменения параметров последнего (например, размера)
    virtual void synchronize(QQuickFramebufferObject * item) override;

    // Создание OpenGlFrameBufferObject'a, через который происходит передача данных
    // Также вызывается при изменении item'a
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;

    // Публикация данных (в данном случае vtk) в OpenGlFrameBufferObject
    virtual void render() override;

    void addActor(vtkActor* actor);
    void removeActor(vtkActor* actor);
    void setData(vtkImageData* data);
    void setSlice(int i, int slice);
    void setWindow(int window);
    void setLevel(int level);

public:
    // Методы для сохранения событий
    void setWheelEvent(QWheelEvent* e);
    void setMouseMoveEvent(QMouseEvent* e);
    void setMousePressEventW(QMouseEvent* e);
    void setMousePressEventL(QMouseEvent* e);
    void setMousePressEventR(QMouseEvent* e);
    void setMouseReleaseEventW(QMouseEvent* e);
    void setMouseReleaseEventL(QMouseEvent* e);
    void setMouseReleaseEventR(QMouseEvent* e);

private:
    void handleEvents();
    void initPlaneViewers();
    void updateWindowLevel();
    void updateSlice();

private:
    // Ссылка на связанный qml объект
    QVTKModelViewerItem* m_item = nullptr;

    // vtk stuff
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_render_window;
    vtkSmartPointer<vtkGenericRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkRenderer> m_renderer;

    vtkSmartPointer<vtkImageData> data;
    vtkSmartPointer<vtkImagePlaneWidget> plane_widget[3];

    int m_window;
    int m_level;
    int m_slice[3];

    bool data_changed = false;
    bool window_level_changed = false;
    bool slice_changed = false;

    // events
    std::shared_ptr<QWheelEvent> m_mouse_wheel;
    std::shared_ptr<QMouseEvent> m_mouse_move;
    std::shared_ptr<QMouseEvent> m_mouse_press_w;
    std::shared_ptr<QMouseEvent> m_mouse_press_l;
    std::shared_ptr<QMouseEvent> m_mouse_press_r;
    std::shared_ptr<QMouseEvent> m_mouse_release_w;
    std::shared_ptr<QMouseEvent> m_mouse_release_l;
    std::shared_ptr<QMouseEvent> m_mouse_release_r;
};

#endif //QVTK_MODEL_VIEWER_H
