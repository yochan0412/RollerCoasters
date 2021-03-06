/************************************************************************
	 File:        TrainView.cpp

	 Author:
				  Michael Gleicher, gleicher@cs.wisc.edu

	 Modifier
				  Yu-Chi Lai, yu-chi@cs.wisc.edu

	 Comment:
						The TrainView is the window that actually shows the
						train. Its a
						GL display canvas (Fl_Gl_Window).  It is held within
						a TrainWindow
						that is the outer window with all the widgets.
						The TrainView needs
						to be aware of the window - since it might need to
						check the widgets to see how to draw

	  Note:        we need to have pointers to this, but maybe not know
						about it (beware circular references)

	 Platform:    Visio Studio.Net 2003/2005

*************************************************************************/

#include <iostream>
#include <Fl/fl.h>

// we will need OpenGL, and OpenGL needs windows.h
#include <windows.h>
//#include "GL/gl.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "GL/glu.h"

#include "TrainView.H"
#include "TrainWindow.H"
#include "Utilities/3DUtils.H"


#ifdef EXAMPLE_SOLUTION
#	include "TrainExample/TrainExample.H"
#endif


//************************************************************************
//
// * Constructor to set up the GL window
//========================================================================
TrainView::
TrainView(int x, int y, int w, int h, const char* l)
	: Fl_Gl_Window(x, y, w, h, l)
	//========================================================================
{
	mode(FL_RGB | FL_ALPHA | FL_DOUBLE | FL_STENCIL);

	resetArcball();
}

//************************************************************************
//
// * Reset the camera to look at the world
//========================================================================
void TrainView::
resetArcball()
//========================================================================
{
	// Set up the camera to look at the world
	// these parameters might seem magical, and they kindof are
	// a little trial and error goes a long way
	arcball.setup(this, 40, 250, .2f, .4f, 0);
}

//************************************************************************
//
// * FlTk Event handler for the window
//########################################################################
// TODO: 
//       if you want to make the train respond to other events 
//       (like key presses), you might want to hack this.
//########################################################################
//========================================================================
int TrainView::handle(int event)
{
	// see if the ArcBall will handle the event - if it does, 
	// then we're done
	// note: the arcball only gets the event if we're in world view
	if (tw->worldCam->value())
		if (arcball.handle(event))
			return 1;

	// remember what button was used
	static int last_push;

	switch (event) {
		// Mouse button being pushed event
	case FL_PUSH:
		last_push = Fl::event_button();
		// if the left button be pushed is left mouse button
		if (last_push == FL_LEFT_MOUSE) {
			doPick();
			damage(1);
			return 1;
		};
		break;

		// Mouse button release event
	case FL_RELEASE: // button release
		damage(1);
		last_push = 0;
		return 1;

		// Mouse button drag event
	case FL_DRAG:

		// Compute the new control point position
		if ((last_push == FL_LEFT_MOUSE) && (selectedCube >= 0)) {
			ControlPoint* cp = &m_pTrack->points[selectedCube];

			double r1x, r1y, r1z, r2x, r2y, r2z;
			getMouseLine(r1x, r1y, r1z, r2x, r2y, r2z);

			double rx, ry, rz;
			mousePoleGo(r1x, r1y, r1z, r2x, r2y, r2z,
				static_cast<double>(cp->pos.x),
				static_cast<double>(cp->pos.y),
				static_cast<double>(cp->pos.z),
				rx, ry, rz,
				(Fl::event_state() & FL_CTRL) != 0);

			cp->pos.x = (float)rx;
			cp->pos.y = (float)ry;
			cp->pos.z = (float)rz;
			damage(1);
		}
		break;

		// in order to get keyboard events, we need to accept focus
	case FL_FOCUS:
		return 1;

		// every time the mouse enters this window, aggressively take focus
	case FL_ENTER:
		focus(this);
		break;

	case FL_KEYBOARD:
		int k = Fl::event_key();
		int ks = Fl::event_state();
		if (k == 'p') {
			// Print out the selected control point information
			if (selectedCube >= 0)
				printf("Selected(%d) (%g %g %g) (%g %g %g)\n",
					selectedCube,
					m_pTrack->points[selectedCube].pos.x,
					m_pTrack->points[selectedCube].pos.y,
					m_pTrack->points[selectedCube].pos.z,
					m_pTrack->points[selectedCube].orient.x,
					m_pTrack->points[selectedCube].orient.y,
					m_pTrack->points[selectedCube].orient.z);
			else
				printf("Nothing Selected\n");

			return 1;
		};
		break;
	}

	return Fl_Gl_Window::handle(event);
}

//************************************************************************
//
// * this is the code that actually draws the window
//   it puts a lot of the work into other routines to simplify things
//========================================================================
void TrainView::draw()
{

	//*********************************************************************
	//
	// * Set up basic opengl informaiton
	//
	//**********************************************************************
	//initialized glad
	if (gladLoadGL())
	{
		//initiailize VAO, VBO, Shader...
	}
	else
		throw std::runtime_error("Could not initialize GLAD!");

	// Set up the view port
	glViewport(0, 0, w(), h());

	// clear the window, be sure to clear the Z-Buffer too
	glClearColor(0, 0, .3f, 0);		// background should be blue

	// we need to clear out the stencil buffer since we'll use
	// it for shadows
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glEnable(GL_DEPTH);

	// Blayne prefers GL_DIFFUSE
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

	// prepare for projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	setProjection();		// put the code to set up matrices here

	//######################################################################
	// TODO: 
	// you might want to set the lighting up differently. if you do, 
	// we need to set up the lights AFTER setting up the projection
	//######################################################################
	// enable the lighting
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	// top view only needs one light
	if (tw->topCam->value()) {
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
	}
	else {
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
	}

	//*********************************************************************
	//
	// * set the light parameters
	//
	//**********************************************************************
	GLfloat lightPosition1[] = { 0,1,1,0 }; // {50, 200.0, 50, 1.0};
	GLfloat lightPosition2[] = { 1, 0, 0, 0 };
	GLfloat lightPosition3[] = { 0, -1, 0, 0 };
	GLfloat yellowLight[] = { 0.5f, 0.5f, .1f, 1.0 };
	GLfloat whiteLight[] = { 1.0f, 1.0f, 1.0f, 1.0 };
	GLfloat blueLight[] = { .1f,.1f,.3f,1.0 };
	GLfloat grayLight[] = { .3f, .3f, .3f, 1.0 };

	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition1);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, whiteLight);
	glLightfv(GL_LIGHT0, GL_AMBIENT, grayLight);

	glLightfv(GL_LIGHT1, GL_POSITION, lightPosition2);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, yellowLight);

	glLightfv(GL_LIGHT2, GL_POSITION, lightPosition3);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, blueLight);



	//*********************************************************************
	// now draw the ground plane
	//*********************************************************************
	// set to opengl fixed pipeline(use opengl 1.x draw function)
	glUseProgram(0);

	setupFloor();
	glDisable(GL_LIGHTING);
	drawFloor(200, 10);


	//*********************************************************************
	// now draw the object and we need to do it twice
	// once for real, and then once for shadows
	//*********************************************************************
	glEnable(GL_LIGHTING);
	setupObjects();

	drawStuff();

	// this time drawing is for shadows (except for top view)
	if (!tw->topCam->value()) {
		setupShadows();
		drawStuff(true);
		unsetupShadows();
	}
}

//************************************************************************
//
// * This sets up both the Projection and the ModelView matrices
//   HOWEVER: it doesn't clear the projection first (the caller handles
//   that) - its important for picking
//========================================================================
void TrainView::
setProjection()
//========================================================================
{
	// Compute the aspect ratio (we'll need it)
	float aspect = static_cast<float>(w()) / static_cast<float>(h());

	// Check whether we use the world camp
	if (tw->worldCam->value())
		arcball.setProjection(false);
	// Or we use the top cam
	else if (tw->topCam->value())
	{
		float wi, he;
		if (aspect >= 1) {
			wi = 110;
			he = wi / aspect;
		}
		else {
			he = 110;
			wi = he * aspect;
		}

		// Set up the top camera drop mode to be orthogonal and set
		// up proper projection matrix
		glMatrixMode(GL_PROJECTION);
		glOrtho(-wi, wi, -he, he, 200, -200);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(-90, 1, 0, 0);
	}
	// Or do the train view or other view here
	//####################################################################
	// TODO: 
	// put code for train view projection here!	
	//####################################################################
	else
	{
		//arcball.setup(this, 40, 250, .2f, .4f, 0);
		Pnt3f pos, dir, up;

		this->getPnt3f(this->t_time, pos, dir, up);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		gluPerspective(40, aspect, 0.1, 1000);


		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		pos = pos + (up * (train_height / 2));


		gluLookAt(pos.x, pos.y, pos.z,
			(dir + pos).x, (dir + pos).y, (dir + pos).z,
			up.x, up.y, up.z);

#ifdef EXAMPLE_SOLUTION
		trainCamView(this, aspect);
#endif
	}
}

//************************************************************************
//
// * this draws all of the stuff in the world
//
//	NOTE: if you're drawing shadows, DO NOT set colors (otherwise, you get 
//       colored shadows). this gets called twice per draw 
//       -- once for the objects, once for the shadows
//########################################################################
// TODO: 
// if you have other objects in the world, make sure to draw them
//########################################################################
//========================================================================
void TrainView::drawStuff(bool doingShadows)
{
	// Draw the control points
	// don't draw the control points if you're driving 
	// (otherwise you get sea-sick as you drive through them)
	if (!tw->trainCam->value())
	{
		for (size_t i = 0; i < m_pTrack->points.size(); i++)
		{
			if (!doingShadows) {
				if (((int)i) != selectedCube)
					glColor3ub(240, 60, 60);
				else
					glColor3ub(240, 240, 30);
			}
			m_pTrack->points[i].draw();
		}
	}
	// draw the track
	//####################################################################
	// TODO: 
	// call your own track drawing code
	//####################################################################

	for (size_t i = 0; i < m_pTrack->points.size(); ++i)
	{
		// pos
		Pnt3f cp_pos_p1;
		Pnt3f cp_pos_p2;

		//dir
		Pnt3f cp_dir;

		// orient
		Pnt3f cp_orient_p1;
		Pnt3f cp_orient_p2;
		float percent = 1.0f / DIVIDE_LINE;
		float t = 0;

		for (size_t j = 0; j < DIVIDE_LINE; j++)
		{
			getPnt3f(percent * (j + 1) + i, cp_pos_p2, cp_dir, cp_orient_p2);
			getPnt3f(percent * j + i, cp_pos_p1, cp_dir, cp_orient_p1);

			glLineWidth(3);
			glBegin(GL_LINES);
			if (!doingShadows)
				glColor3ub(32, 32, 64);
			glVertex3f(cp_pos_p1.x, cp_pos_p1.y, cp_pos_p1.z);
			glVertex3f(cp_pos_p2.x, cp_pos_p2.y, cp_pos_p2.z);
			glEnd();
			glLineWidth(1);

			// cross
			Pnt3f cross_t = (cp_dir)*cp_orient_p1;
			cross_t.normalize();
			cross_t = cross_t * 2.5f;

			glBegin(GL_LINES);
			glVertex3f(cp_pos_p1.x + cross_t.x, cp_pos_p1.y + cross_t.y, cp_pos_p1.z + cross_t.z);
			glVertex3f(cp_pos_p2.x + cross_t.x, cp_pos_p2.y + cross_t.y, cp_pos_p2.z + cross_t.z);
			glVertex3f(cp_pos_p1.x - cross_t.x, cp_pos_p1.y - cross_t.y, cp_pos_p1.z - cross_t.z);
			glVertex3f(cp_pos_p2.x - cross_t.x, cp_pos_p2.y - cross_t.y, cp_pos_p2.z - cross_t.z);
			glEnd();
			glLineWidth(1);

			if (!doingShadows)
				glColor3ub(255, 255, 255);
			//break;
		}
		for (size_t j = 0; j < DIVIDE_LINE; j++)
		{
			getPnt3f(percent * j + i, cp_pos_p1, cp_dir, cp_orient_p1);
			Pnt3f u = cp_dir;
			Pnt3f w = u * cp_orient_p1;
			w.normalize();
			Pnt3f v = w * u;
			v.normalize();


			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glTranslatef(cp_pos_p1.x, cp_pos_p1.y, cp_pos_p1.z);

			float rotation[16] = {
									u.x, u.y, u.z, 0.0,
									v.x, v.y, v.z, 0.0,
									w.x, w.y, w.z, 0.0,
									0.0, 0.0, 0.0, 1.0
			};

			glMultMatrixf(rotation);

			glBegin(GL_QUADS);

			float C1 = 0.75, C2 = 3;

			if (!doingShadows)
				glColor3ub(255, 100, 0);
			glNormal3f(0, -1, 0); //Bottom
			glVertex3f(-C1, -C1, -C2);
			glVertex3f(C1, -C1, -C2);
			glVertex3f(C1, -C1, C2);
			glVertex3f(-C1, -C1, C2);

			if (!doingShadows)
				glColor3ub(0, 200, 255);
			glNormal3f(0, 1, 0); //Up
			glVertex3f(-C1, C1, -C2);
			glVertex3f(C1, C1, -C2);
			glVertex3f(C1, C1, C2);
			glVertex3f(-C1, C1, C2);

			if (!doingShadows)
				glColor3ub(255, 255, 255);
			glNormal3f(-1, 0, 0); //Left
			glVertex3f(-C1, C1, -C2);
			glVertex3f(-C1, C1, C2);
			glVertex3f(-C1, -C1, C2);
			glVertex3f(-C1, -C1, -C2);

			if (!doingShadows)
				glColor3ub(255, 255, 255);
			glNormal3f(1, 0, 0); //Right
			glVertex3f(C1, C1, -C2);
			glVertex3f(C1, C1, C2);
			glVertex3f(C1, -C1, C2);
			glVertex3f(C1, -C1, -C2);

			if (!doingShadows)
				glColor3ub(255, 255, 255);
			glNormal3f(0, 0, 1); //Front
			glVertex3f(-C1, C1, C2);
			glVertex3f(C1, C1, C2);
			glVertex3f(C1, -C1, C2);
			glVertex3f(-C1, -C1, C2);

			if (!doingShadows)
				glColor3ub(255, 255, 255);
			glNormal3f(0, 0, -1); //Behind
			glVertex3f(-C1, C1, -C2);
			glVertex3f(C1, C1, -C2);
			glVertex3f(C1, -C1, -C2);
			glVertex3f(-C1, -C1, -C2);


			glEnd();
			glPopMatrix();
		}
		//break;
	}

	// draw the train
	//####################################################################
	// TODO: 
	//	call your own train drawing code
	//####################################################################

	if (!this->tw->trainCam->value())
	{
		Pnt3f train_pos, train_dir, train_orient;
		getPnt3f(this->t_time, train_pos, train_dir, train_orient);
		Pnt3f u = train_dir;
		Pnt3f w = u * train_orient;
		w.normalize();
		Pnt3f v = w * u;
		v.normalize();


		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glTranslatef(train_pos.x, train_pos.y, train_pos.z);

		float rotation[16] = {
								u.x, u.y, u.z, 0.0,
								v.x, v.y, v.z, 0.0,
								w.x, w.y, w.z, 0.0,
								0.0, 0.0, 0.0, 1.0
		};

		glMultMatrixf(rotation);
		glRotatef(90, 0, -1, 0);
		glTranslatef(0, 0.75, 0);
		glBegin(GL_QUADS);

		if (!doingShadows)
			glColor3ub(255, 255, 255);
		glNormal3f(0, -1, 0); //Bottom
		glVertex3f(-train_width / 2, 0, -train_length / 2);
		glVertex3f(train_width / 2, 0, -train_length / 2);
		glVertex3f(train_width / 2, 0, train_length / 2);
		glVertex3f(-train_width / 2, 0, train_length / 2);

		if (!doingShadows)
			glColor3ub(0, 0, 0);
		glNormal3f(0, 1, 0); //top
		glVertex3f(-train_width / 2, train_height, -train_length / 2);
		glVertex3f(train_width / 2, train_height, -train_length / 2);
		glVertex3f(train_width / 2, train_height, train_length / 2);
		glVertex3f(-train_width / 2, train_height, train_length / 2);

		if (!doingShadows)
			glColor3ub(255, 0, 0);
		glNormal3f(-1, 0, 0); //Left
		glVertex3f(-train_width / 2, train_height, -train_length / 2);
		glVertex3f(-train_width / 2, train_height, train_length / 2);
		glVertex3f(-train_width / 2, 0, train_length / 2);
		glVertex3f(-train_width / 2, 0, -train_length / 2);

		if (!doingShadows)
			glColor3ub(255, 0, 0);
		glNormal3f(1, 0, 0); //Right
		glVertex3f(train_width / 2, train_height, -train_length / 2);
		glVertex3f(train_width / 2, train_height, train_length / 2);
		glVertex3f(train_width / 2, 0, train_length / 2);
		glVertex3f(train_width / 2, 0, -train_length / 2);

		if (!doingShadows)
			glColor3ub(0, 255, 0);
		glNormal3f(0, 0, 1); //Front
		glVertex3f(-train_width / 2, train_height, -train_length / 2);
		glVertex3f(train_width / 2, train_height, -train_length / 2);
		glVertex3f(train_width / 2, 0, -train_length / 2);
		glVertex3f(-train_width / 2, 0, -train_length / 2);

		if (!doingShadows)
			glColor3ub(0, 0, 255);
		glNormal3f(0, 0, -1); //Behind
		glVertex3f(-train_width / 2, train_height, train_length / 2);
		glVertex3f(train_width / 2, train_height, train_length / 2);
		glVertex3f(train_width / 2, 0, train_length / 2);
		glVertex3f(-train_width / 2, 0, train_length / 2);


		glEnd();
		glPopMatrix();
	}
}



// 
//************************************************************************
//
// * this tries to see which control point is under the mouse
//	  (for when the mouse is clicked)
//		it uses OpenGL picking - which is always a trick
//########################################################################
// TODO: 
//		if you want to pick things other than control points, or you
//		changed how control points are drawn, you might need to change this
//########################################################################
//========================================================================
void TrainView::doPick()
//========================================================================
{
	// since we'll need to do some GL stuff so we make this window as 
	// active window
	make_current();

	// where is the mouse?
	int mx = Fl::event_x();
	int my = Fl::event_y();

	// get the viewport - most reliable way to turn mouse coords into GL coords
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	// Set up the pick matrix on the stack - remember, FlTk is
	// upside down!
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPickMatrix((double)mx, (double)(viewport[3] - my),
		5, 5, viewport);

	// now set up the projection
	setProjection();

	// now draw the objects - but really only see what we hit
	GLuint buf[100];
	glSelectBuffer(100, buf);
	glRenderMode(GL_SELECT);
	glInitNames();
	glPushName(0);

	// draw the cubes, loading the names as we go
	for (size_t i = 0; i < m_pTrack->points.size(); ++i) {
		glLoadName((GLuint)(i + 1));
		m_pTrack->points[i].draw();
	}

	// go back to drawing mode, and see how picking did
	int hits = glRenderMode(GL_RENDER);
	if (hits) {
		// warning; this just grabs the first object hit - if there
		// are multiple objects, you really want to pick the closest
		// one - see the OpenGL manual 
		// remember: we load names that are one more than the index
		selectedCube = buf[3] - 1;
	}
	else // nothing hit, nothing selected
		selectedCube = -1;

	printf("Selected Cube %d\n", selectedCube);
}

void TrainView::getPnt3f(float t, Pnt3f& pos, Pnt3f& dir, Pnt3f& up)
{
	while (t >= m_pTrack->points.size())
	{
		t -= m_pTrack->points.size();
	}
	if (this->tw->splineBrowser->selected(1))
	{
		for (int i = 0; i < this->m_pTrack->points.size(); i++)
		{
			if (t >= i && t < i + 1)
			{
				t -= i;
				pos = t * m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos + (1 - t) * m_pTrack->points[i].pos;
				dir = m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos - m_pTrack->points[i].pos;
				dir.normalize();
				up = t * m_pTrack->points[(i + 1) % m_pTrack->points.size()].orient + (1 - t) * m_pTrack->points[i].orient;
				up.normalize();
				break;
			}
		}
	}
	else if (this->tw->splineBrowser->selected(2))
	{
		const float M_matrix[4][4] = {
										-1.0,  2.0, -1.0,  0.0,
										 3.0, -5.0,  0.0,  2.0,
										-3.0,  4.0,  1.0,  0.0,
										 1.0,  -1.0,  0.0,  0.0
		};

		for (int i = 0; i < this->m_pTrack->points.size(); i++)
		{
			if (t >= i && t < i + 1)
			{
				t -= i;
				const float T[4] = { t * t * t,t * t,t,1.0 };
				const float DT[4] = { 3.0 * t * t,2.0 * t,1.0,0 };
				Pnt3f G[4];
				if (i == 0)
				{
					G[0] = m_pTrack->points[m_pTrack->points.size() - 1].pos;
				}
				else
				{
					G[0] = m_pTrack->points[i - 1].pos;
				}
				G[1] = m_pTrack->points[(i) % m_pTrack->points.size()].pos;
				G[2] = m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos;
				G[3] = m_pTrack->points[(i + 2) % m_pTrack->points.size()].pos;
				pos = Matrix_Multiple(M_matrix, T, G, 0.5);
				dir = Matrix_Multiple(M_matrix, DT, G, 0.5);
				dir.normalize();
				G[0] = m_pTrack->points[i].orient;
				G[1] = m_pTrack->points[(i + 1) % m_pTrack->points.size()].orient;
				G[2] = m_pTrack->points[(i + 2) % m_pTrack->points.size()].orient;
				G[3] = m_pTrack->points[(i + 3) % m_pTrack->points.size()].orient;
				up = Matrix_Multiple(M_matrix, T, G, 0.5);
				up.normalize();
			}
		}
	}
	else if (this->tw->splineBrowser->selected(3))
	{
		const float M_matrix[4][4] = {
										-1.0,  3.0, -3.0,  1.0,
										 3.0, -6.0,  0.0,  4.0,
										-3.0,  3.0,  3.0,  1.0,
										 1.0,  0.0,  0.0,  0.0
		};

		for (int i = 0; i < this->m_pTrack->points.size(); i++)
		{
			if (t >= i && t < i + 1)
			{
				t -= i;
				const float T[4] = { t * t * t,t * t,t,1.0 };
				const float DT[4] = { 3.0 * t * t,2.0 * t,1.0,0 };
				Pnt3f G[4];
				if (i == 0)
				{
					G[0] = m_pTrack->points[m_pTrack->points.size() - 1].pos;
				}
				else
				{
					G[0] = m_pTrack->points[i - 1].pos;
				}
				G[1] = m_pTrack->points[(i) % m_pTrack->points.size()].pos;
				G[2] = m_pTrack->points[(i + 1) % m_pTrack->points.size()].pos;
				G[3] = m_pTrack->points[(i + 2) % m_pTrack->points.size()].pos;
				pos = Matrix_Multiple(M_matrix, T, G, 1.0f / 6.0f);
				dir = Matrix_Multiple(M_matrix, DT, G, 1.0f / 6.0f);
				dir.normalize();
				if (i == 0)
				{
					G[0] = m_pTrack->points[m_pTrack->points.size() - 1].orient;
				}
				else
				{
					G[0] = m_pTrack->points[i - 1].orient;
				}
				G[1] = m_pTrack->points[(i + 1) % m_pTrack->points.size()].orient;
				G[2] = m_pTrack->points[(i + 2) % m_pTrack->points.size()].orient;
				G[3] = m_pTrack->points[(i + 3) % m_pTrack->points.size()].orient;
				up = Matrix_Multiple(M_matrix, T, G, 1.0f / 6.0f);
				up.normalize();
			}
		}
	}

	return;
}

Pnt3f Matrix_Multiple(const float matrix[4][4], const float T_var[4], Pnt3f* g, float r)
{
	Pnt3f temp;
	float result[4] = { 0 };

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			result[i] += (matrix[i][j] * T_var[j]);
		}
	}

	for (int i = 0; i < 4; i++)
	{
		temp = temp + (g[i] * result[i]);
	}

	return temp * r;
}