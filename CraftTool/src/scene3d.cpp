#include "scene3d.h"
#include <QWheelEvent>
#include "log.h"
//#include "ui_mainwindow.h"

//--------------------------------------------------------------------
Scene3d::Scene3d(QWidget *parent) : QGLWidget(parent)
{
    m_zoneWidth = 2000;
    m_zoneHeight = 1000;
    m_zoneTop = 100;

    m_gridStep = 100;
    m_showGrid = true;

    m_scale = 1;
    m_cameraPosition = glm::vec3(0,0,2000);  //находится сверху
    m_cameraLook     = glm::vec3(0,0,-1); //смотрит вниз
    m_cameraTop      = glm::vec3(0,1,0);  //смотрит ровно

    m_lastMousePosition = QPoint(0,0);
    m_mousePressed = false;

    m_windowWidth = 0;
    m_windowHeight = 0;
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
    glm::mat4 mView  = glm::lookAt(m_cameraPosition,
                                   m_cameraPosition + m_cameraLook,
                                   m_cameraTop);

    float scale = m_scale * 1000; //1пиксель - 1мм
    glm::mat4 mScale = glm::scale(glm::mat4(), glm::vec3(scale/m_windowWidth, scale/m_windowHeight, 1.f));

    glm::mat4 mProj  = glm::perspective(glm::pi<float>()/4, 1.f, 0.01f, 10000.0f);

    m_viewProjection = mProj * mScale * mView;// * mScale;
    glLoadMatrixf(glm::value_ptr(m_viewProjection));
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
        float deltaX = scale * float(pe->x() - m_lastMousePosition.x()) / width();
        float deltaY = scale * float(pe->y() - m_lastMousePosition.y()) / height();

        glm::vec3 axisX(1,0,0), axisY(0,1,0);
        glm::mat4 rotation;
        rotation = glm::rotate(rotation, deltaX, m_cameraTop);
        rotation = glm::rotate(rotation, deltaY, glm::cross(m_cameraLook, m_cameraTop));

        m_cameraLook = glm::vec3(glm::vec4(m_cameraLook,0) * rotation);
        m_cameraPosition = glm::vec3(glm::vec4(m_cameraPosition,0) * rotation);
        m_cameraTop = glm::vec3(glm::vec4(m_cameraTop,0) * rotation);

        recalc_matrices();
        updateGL();

        m_lastMousePosition = pe->pos();
    }
}

//--------------------------------------------------------------------
void Scene3d::wheelEvent(QWheelEvent* pe)
{
    if (pe->delta() > 0)
        m_scale *= 2;
    else if (pe->delta() < 0)
        m_scale /= 2;

    recalc_matrices();
    updateGL();
    log_message("%f\n", m_scale);
}

//--------------------------------------------------------------------
void Scene3d::resizeGL(int nWidth, int nHeight)
{
    glViewport(0,0,nWidth,nHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);

    m_windowWidth = nWidth;
    m_windowHeight = nHeight;

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

    draw_bounds();

    draw_grid();

    glFlush();
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
        glVertex3f(0,0,m_zoneTop);

        glVertex3f(m_zoneWidth,0,0);
        glVertex3f(m_zoneWidth,m_zoneHeight,0);
        glVertex3f(m_zoneWidth,0,0);
        glVertex3f(m_zoneWidth,0,m_zoneTop);

        glVertex3f(0,m_zoneHeight,0);
        glVertex3f(m_zoneWidth,m_zoneHeight,0);
        glVertex3f(0,m_zoneHeight,0);
        glVertex3f(0,m_zoneHeight,m_zoneTop);

        glVertex3f(0,0,m_zoneTop);
        glVertex3f(m_zoneWidth,0,m_zoneTop);
        glVertex3f(0,0,m_zoneTop);
        glVertex3f(0,m_zoneHeight,m_zoneTop);

        glVertex3f(m_zoneWidth,m_zoneHeight,0);
        glVertex3f(m_zoneWidth,m_zoneHeight,m_zoneTop);
        glVertex3f(m_zoneWidth,0,m_zoneTop);
        glVertex3f(m_zoneWidth,m_zoneHeight,m_zoneTop);
        glVertex3f(0,m_zoneHeight,m_zoneTop);
        glVertex3f(m_zoneWidth,m_zoneHeight,m_zoneTop);
    glEnd();
}

//--------------------------------------------------------------------
void Scene3d::draw_grid()
{
    glColor3f(0.3, 0.0, 0.0);

    glBegin(GL_LINES);
        for(float x = 0; x < m_zoneWidth; x += m_gridStep)
        {
            glVertex3f(x,0,0);
            glVertex3f(x,m_zoneHeight,0);
        }

        for(float y = 0; y < m_zoneHeight; y += m_gridStep)
        {
            glVertex3f(0,y,0);
            glVertex3f(m_zoneWidth,y,0);
        }
    glEnd();
}
