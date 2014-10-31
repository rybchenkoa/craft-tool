#include <QtOpenGL/QGLWidget>

namespace Ui
{
    class Scene3d;
}

class Scene3d :  public QGLWidget
{
    Q_OBJECT

public:
    Scene3d(QWidget *parent = 0);
    ~Scene3d();

protected:
    void initializeGL();
    void resizeGL(int nWidth, int nHeight);
    void paintGL();
    /*void mousePressEvent(QMouseEvent* pe);
    void mouseMoveEvent(QMouseEvent* pe);
    void mouseReleaseEvent(QMouseEvent* pe);
    void wheelEvent(QWheelEvent* pe);
    void keyPressEvent(QKeyEvent* pe);*/
};
