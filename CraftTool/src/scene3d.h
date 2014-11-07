#include <QtOpenGL/QGLWidget>
#include <QPoint>
#include "StdAfx.h"

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
    void mousePressEvent(QMouseEvent* pe);
    void mouseMoveEvent(QMouseEvent* pe);
    void mouseReleaseEvent(QMouseEvent* pe);
    void wheelEvent(QWheelEvent* pe);
    //void keyPressEvent(QKeyEvent* pe);

    void recalc_matrices();
    void draw_bounds();
    void draw_grid();

public:
    int m_windowWidth;  //размеры окна
    int m_windowHeight;

    float m_zoneWidth;  //размеры зоны станка
    float m_zoneHeight;
    float m_zoneTop;

    bool m_showGrid;
    float m_gridStep;   //размер ячейки сетки

    QPoint m_lastMousePosition;
    bool m_mousePressed;

    float m_scale;              //масштаб изображения
    glm::vec3 m_cameraPosition; //где находится камера
    glm::vec3 m_cameraLook;     //нормализованный вектор взгляда
    glm::vec3 m_cameraTop;      //вектор ориентации камеры
    float m_screenAngle;        //поворот экрана
    glm::mat4 m_viewProjection; //матрица камеры
};
