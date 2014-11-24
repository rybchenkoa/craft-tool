#include "scene3d.h"
#include <QWheelEvent>
#include "log.h"
//#include "ui_mainwindow.h"

//--------------------------------------------------------------------
Scene3d::Scene3d(QWidget *parent) : QGLWidget(parent)
{
    m_zoneWidth = 2000;
    m_zoneHeight = 1000;
    m_zoneTop = 300;

    m_gridStep = 100;
    m_showGrid = true;

    camera.scale = 1;
    camera.position = glm::vec3(0,0,2000);  //находится сверху
    camera.look     = glm::vec3(0,0,-1); //смотрит вниз
    camera.top      = glm::vec3(0,1,0);  //смотрит ровно

    m_lastMousePosition = QPoint(0,0);
    m_mousePressed = false;

    m_windowWidth = 0;
    m_windowHeight = 0;

    make_tool_simple(tool); //делаем квадратное сверло )
}

//--------------------------------------------------------------------
Scene3d::~Scene3d()
{
    //;
}

//--------------------------------------------------------------------
void Scene3d::initializeGL()
{
   qglClearColor(Qt::black);
   glEnable(GL_DEPTH_TEST);
   glShadeModel(GL_FLAT);
   glEnable(GL_CULL_FACE);

   /*getVertexArray();
   getColorArray();
   getIndexArray();

   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_COLOR_ARRAY);*/
}

void Scene3d::recalc_matrices()
{
    camera.recalc_matrix(m_windowWidth, m_windowHeight);
}

//--------------------------------------------------------------------
void Scene3d::mousePressEvent(QMouseEvent* pe)
{
    m_lastMousePosition = pe->pos();
    m_mousePressed = true;
}

//--------------------------------------------------------------------
void Scene3d::mouseReleaseEvent(QMouseEvent* pe)
{
    Q_UNUSED(pe)
    m_mousePressed = false;
}

//--------------------------------------------------------------------
void Scene3d::mouseMoveEvent(QMouseEvent* pe)
{
    if(m_mousePressed)
    {
        float scale = 5.1;
        float x = (float(m_lastMousePosition.x()) / width() - 0.5)*2;
        float y = (1 - float(m_lastMousePosition.y()) / height() - 0.5)*2;
        float newX = (float(pe->x()) / width() - 0.5)*2;
        float newY = (1 - float(pe->y()) / height() - 0.5)*2;
        float deltaX = scale * (newX - x);
        float deltaY = scale * (newY - y);

        camera.rotate_cursor(x, y, deltaX, deltaY);
        recalc_matrices();
        updateGL();

        m_lastMousePosition = pe->pos();
    }
}

//--------------------------------------------------------------------
void Scene3d::wheelEvent(QWheelEvent* pe)
{
    if (pe->delta() > 0)
        camera.scale *= 2;
    else if (pe->delta() < 0)
        camera.scale /= 2;

    recalc_matrices();
    updateGL();
    log_message("%f\n", camera.scale);
}

//--------------------------------------------------------------------
void Scene3d::resizeGL(int nWidth, int nHeight)
{
    m_windowWidth = nWidth;
    m_windowHeight = nHeight;

    glViewport(0,0,nWidth,nHeight);

    glMatrixMode(GL_PROJECTION);

    recalc_matrices();
}

//--------------------------------------------------------------------
void Scene3d::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glColor3f(0.0, 1.0, 1.0);

    /*glBegin(GL_POINTS);
        glVertex2f(0.05,0.05);
        glVertex2f(0.5,0.5);
    glEnd();*/

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    draw_bounds();

    draw_grid();

    draw_track();

    tool.draw();

    glFlush();
}

//--------------------------------------------------------------------
void Scene3d::draw_track()
{
    glBegin(GL_LINE_STRIP);
    for(size_t i = 0; i < track.size(); ++i)
    {
        glColor3f(track[i].color.r,track[i].color.g,track[i].color.b);
        glVertex3f(track[i].position.x, track[i].position.y, track[i].position.z);
    }
    glEnd();
}

//--------------------------------------------------------------------
void Scene3d::draw_bounds()
{
    glColor3f(0.0, 0.3, 0.3);

    glBegin(GL_LINES);
        glVertex3f(0,0,0);
        glVertex3f(m_zoneWidth,0,0);
        glVertex3f(0,0,0);
        glVertex3f(0,m_zoneHeight,0);
        glVertex3f(0,0,0);
        glVertex3f(0,0,-m_zoneTop);

        glVertex3f(m_zoneWidth,0,0);
        glVertex3f(m_zoneWidth,m_zoneHeight,0);
        glVertex3f(m_zoneWidth,0,0);
        glVertex3f(m_zoneWidth,0,-m_zoneTop);

        glVertex3f(0,m_zoneHeight,0);
        glVertex3f(m_zoneWidth,m_zoneHeight,0);
        glVertex3f(0,m_zoneHeight,0);
        glVertex3f(0,m_zoneHeight,-m_zoneTop);

        glVertex3f(0,0,-m_zoneTop);
        glVertex3f(m_zoneWidth,0,-m_zoneTop);
        glVertex3f(0,0,-m_zoneTop);
        glVertex3f(0,m_zoneHeight,-m_zoneTop);

        glVertex3f(m_zoneWidth,m_zoneHeight,0);
        glVertex3f(m_zoneWidth,m_zoneHeight,-m_zoneTop);
        glVertex3f(m_zoneWidth,0,-m_zoneTop);
        glVertex3f(m_zoneWidth,m_zoneHeight,-m_zoneTop);
        glVertex3f(0,m_zoneHeight,-m_zoneTop);
        glVertex3f(m_zoneWidth,m_zoneHeight,-m_zoneTop);
    glEnd();
}

//--------------------------------------------------------------------
void Scene3d::draw_grid()
{
    glColor3f(0.3, 0.0, 0.0);

    glBegin(GL_LINES);
        for(float x = 0; x < m_zoneWidth; x += m_gridStep)
        {
            glVertex3f(x,0,-m_zoneTop);
            glVertex3f(x,m_zoneHeight,-m_zoneTop);
        }

        for(float y = 0; y < m_zoneHeight; y += m_gridStep)
        {
            glVertex3f(0,y,-m_zoneTop);
            glVertex3f(m_zoneWidth,y,-m_zoneTop);
        }
    glEnd();
}

//--------------------------------------------------------------------
void Scene3d::update_tool_coords(float x, float y, float z)
{
    tool.position = glm::vec3(x,y,z);
    TrackPoint point;
    point.color = glm::vec3(0.5f, 0.5f + (rand() % 500)/1000.f, 0);
    point.position = tool.position;
    track.push_back(point);
    updateGL();
}

//--------------------------------------------------------------------
void Camera::recalc_matrix(int width, int height)
{
    glm::mat4 mView  = glm::lookAt(position,
                                   position + look,
                                   top);

    float fscale = scale * 1000; //1пиксель - 1мм
    glm::mat4 mScale = glm::scale(glm::mat4(), glm::vec3(fscale/width, fscale/height, 1.f));

    glm::mat4 mProj  = glm::perspective(glm::pi<float>()/4, 1.f, 0.01f, 10000.0f);

    viewProjection = mProj * mScale * mView;// * mScale;

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(viewProjection));
}

//--------------------------------------------------------------------
void Camera::rotate_cursor(float x, float y, float deltaX, float deltaY)
{
    glm::vec3 axisX = glm::cross(look, top);
    glm::vec3 axisY = top;
    glm::vec3 axisZ = -look;

    //считаем ось в пространстве камеры,
    //потом переводим её в глобальное пространство
    //и поворачиваем вектора камеры относительно этой оси
    glm::vec3 axisToCursor(x, y, glm::sqrt(glm::abs(1 - x*x - y*y)));
    glm::vec3 cursorOffset(deltaX, deltaY, 0);
    glm::vec3 axis = glm::cross(axisToCursor, cursorOffset);
    glm::vec3 localAxis = axis.x * axisX + axis.y * axisY + axis.z * axisZ;

    glm::mat4 rotation;
    rotation = glm::rotate(rotation, glm::sqrt(deltaX*deltaX + deltaY*deltaY), localAxis);

    look = glm::vec3(glm::vec4(look,0) * rotation);
    position = glm::vec3(glm::vec4(position,0) * rotation);
    top = glm::vec3(glm::vec4(top,0) * rotation);
}

//--------------------------------------------------------------------
void Object3d::draw()
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);

    glBegin(GL_TRIANGLES);
    for(size_t i = 0; i < indices.size(); ++i)
    {
        auto &vert = verts[indices[i]];
        glColor3fv(&vert.color.x);
        glVertex3fv(&vert.position.x);
    }
    glEnd();

    glPopMatrix();
}

//--------------------------------------------------------------------
//из границы в плоскости XZ создаёт объект вращения вокруг z
void make_cylinder(Object3d& edge, int divs)
{
    int countPoints = edge.verts.size();

    auto push_quad = [&edge, &countPoints](int x1, int x2, int y1, int y2)
    {
        edge.indices.push_back(x1 * countPoints + y1);
        edge.indices.push_back(x1 * countPoints + y2);
        edge.indices.push_back(x2 * countPoints + y1);


        edge.indices.push_back(x2 * countPoints + y2);
        edge.indices.push_back(x2 * countPoints + y1);
        edge.indices.push_back(x1 * countPoints + y2);
    };

    for(int i = 1; i < divs; ++i)
    {
        float phi = float(i) / divs * 2 * glm::pi<float>();
        float cosPhi = std::cos(phi);
        float sinPhi = std::sin(phi);

        for(int j = 0; j < countPoints; ++j)
        {
            auto &from = edge.verts[j];
            glm::vec3 pos;
            pos.x = from.position.x * cosPhi - from.position.y * sinPhi;
            pos.y = from.position.y * cosPhi + from.position.x * sinPhi;
            pos.z = from.position.z;
            glm::vec3 color((rand()%1000)/1000.0,0,0);
            Vertex to(pos, color);

            edge.verts.push_back(to);
            if(j != 0)
                push_quad(i - 1, i, j-1, j);
        }
    }

    for(int j = 0; j < countPoints; ++j)
        push_quad(divs - 1, 0, j, j + 1);
}

//--------------------------------------------------------------------
//создаёт объект сверло
void make_tool_simple(Object3d& tool)
{
    tool.ortX = glm::vec3(1,0,0);
    tool.ortY = glm::vec3(0,1,0);
    tool.indices.clear();
    tool.verts.clear();
    tool.position = glm::vec3(0,0,0);

    glm::vec3 simple[] =
    {
        {0,  0, 0},
        {10, 0, 0},
        {10, 0, -95},
        {0,  0, -100}
    };

    //glm::vec3 color(1,0,0);
    for(int i = 0; i < _countof(simple); ++i)
    {
        glm::vec3 color((rand()%1000)/1000.0,0,0);
        Vertex vert(simple[i], color);
        tool.verts.push_back(vert);
    }

    make_cylinder(tool, 4);
}
