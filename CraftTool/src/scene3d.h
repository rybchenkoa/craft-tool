#include <QtOpenGL/QGLWidget>
#include <QPoint>
#include "StdAfx.h"

namespace Ui
{
    class Scene3d;
}

struct TrackPoint
{
    glm::vec3 position;
    glm::vec3 color;
};

struct Camera
{
    float     scale;          //масштаб изображени€
    glm::vec3 position;       //где находитс€ камера
    glm::vec3 look;           //нормализованный вектор взгл€да
    glm::vec3 top;            //вектор ориентации камеры
    float     screenAngle;    //поворот экрана
    glm::mat4 viewProjection; //матрица камеры

    void recalc_matrix(int width, int height);     //пересчитать матрицу проекции
    void rotate_cursor(float x, float y, float deltaX, float deltaY); //обработка поворота камеры
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;

    Vertex(glm::vec3 _position, glm::vec3 _color):
        position(_position), color(_color) {};
};

struct Object3d
{
    glm::vec3           position; //положение
    glm::vec3           ortX;     //ориентаци€
    glm::vec3           ortY;
    std::vector<Vertex> verts;    //вершины
    std::vector<int>    indices;  //треугольники

    void draw(); //нарисовать треугольники
};

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
    void draw_track();
    void draw_border();

public:
    int    m_windowWidth;  //размеры окна
    int    m_windowHeight;

    float  m_zoneWidth;  //размеры зоны станка
    float  m_zoneHeight;
    float  m_zoneTop;

    bool   m_showGrid;
    float  m_gridStep;   //размер €чейки сетки

    QPoint m_lastMousePosition;
    bool   m_mousePressed;

    Camera camera;
    Object3d tool;

    std::vector<TrackPoint> track; //траектори€ фрезы

public slots:
    void update_tool_coords(float x, float y, float z);
};

void make_cylinder(Object3d& edge, int divs); //из границы в плоскости XZ создаЄт объект вращени€ вокруг z

void make_tool_simple(Object3d& tool); //создаЄт объект сверло
