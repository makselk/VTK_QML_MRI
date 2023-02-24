#ifndef QVTK_PLANE_VIEWER_H
#define QVTK_PLANE_VIEWER_H


#include <QtQuick/QQuickFramebufferObject>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QQuickFramebufferObject>
#include <QOpenGLFramebufferObject>

#include <vtkSmartPointer.h>
#include <vtkGenericRenderWindowInteractor.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkResliceImageViewer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>


class QVTKPlaneViewerRenderer;


// Непосредственно сам объект, который видит QML
class QVTKPlaneViewerItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(int orientation READ getOrientation WRITE setOrientation NOTIFY orientationChanged)
public:
    int orientation = 0;
    int getOrientation() const;
    void setOrientation(const int&);
signals:
    void orientationChanged();

public:
    QVTKPlaneViewerItem();
    ~QVTKPlaneViewerItem() = default;

    // После создания объекта, qml просит создать рендерер,
    // который будет предоставлять OpenGL инофрмацию
    QQuickFramebufferObject::Renderer *createRenderer() const override;
    // Проверка наличия инициализированного поля рендерера
    bool isInitialized() const;
    // Инициализирует поле рендерера
    void setFboRenderer(QVTKPlaneViewerRenderer* renderer);
    // Возвращает указатель на рендерер, это нужно, тк через него
    // осуществляются все действия с данными
    QVTKPlaneViewerRenderer* getRenderer();

public:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

protected:
    QVTKPlaneViewerRenderer* m_fbo_renderer = nullptr;
};



// Объект, в котором происходит вся магия передачи изображения
// от VTK к QML через OpenGLFrameBufferObject
class QVTKPlaneViewerRenderer :
        public QObject,
        public QQuickFramebufferObject::Renderer,
        protected QOpenGLFunctions {

public:
    QVTKPlaneViewerRenderer();
    ~QVTKPlaneViewerRenderer() = default;

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

    // Обновление данных мрт/кт
    void setData(vtkImageData* data);

    // Обновление параметров m_image_viewer
    void setSlice(int slice);
    void setWindow(int value);
    void setLevel(int value);
    void pickingOn()  {picking_enabled = true;}
    void pickingOff() {picking_enabled = false;}

public:
    // Методы для сохранения событий
    void setMouseMoveEvent(QMouseEvent* e);
    void setMousePressEventW(QMouseEvent* e);
    void setMousePressEventL(QMouseEvent* e);
    void setMousePressEventR(QMouseEvent* e);
    void setMouseReleaseEventW(QMouseEvent* e);
    void setMouseReleaseEventL(QMouseEvent* e);
    void setMouseReleaseEventR(QMouseEvent* e);

private:
    // Инициализация сцены (первоначальная, либо после updateData)
    void updateData();
    // Обработка накопившихся событий
    void handleEvents();

private:
    bool picking_enabled = false;
    bool data_changed = false;
    bool interaction = false;
    bool slice_changed = false;
    bool window_changed = false;
    bool level_changed = false;

    int m_slice = 0;
    int m_window = 0;
    int m_level = 0;

    // Ссылка на связанный qml объект
    QVTKPlaneViewerItem* m_item = nullptr;

    // vtk stuff
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_render_window;
    vtkSmartPointer<vtkRenderWindowInteractor> m_interactor;
    vtkSmartPointer<vtkResliceImageViewer> m_image_viewer;
    vtkSmartPointer<vtkRenderer> m_renderer;
    vtkImageData* data = nullptr;

    // events
    std::shared_ptr<QMouseEvent> m_mouse_move;
    std::shared_ptr<QMouseEvent> m_mouse_press_w;
    std::shared_ptr<QMouseEvent> m_mouse_press_l;
    std::shared_ptr<QMouseEvent> m_mouse_press_r;
    std::shared_ptr<QMouseEvent> m_mouse_release_w;
    std::shared_ptr<QMouseEvent> m_mouse_release_l;
    std::shared_ptr<QMouseEvent> m_mouse_release_r;
};

#endif //QVTK_PLANE_VIEWER_H
