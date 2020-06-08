#include "Lens.h"


Lens::Lens()
{
	m_bQuit = false;
}

Lens::~Lens()
{
	m_bQuit = true;
}
