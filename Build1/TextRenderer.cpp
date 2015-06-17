#include "TextRenderer.h"
#include <stdio.h>
#include <stdarg.h>
#include <OgreOverlayManager.h>


template<> TextRenderer* Ogre::Singleton<TextRenderer>::msSingleton = 0; 

TextRenderer::TextRenderer()
{
	_overlayMgr = Ogre::OverlayManager::getSingletonPtr();

	_overlay = _overlayMgr->create("overlay1");
	_panel = static_cast<Ogre::OverlayContainer*>(_overlayMgr->createOverlayElement("Panel", "container"));
	_panel->setDimensions(0.2, 0.2);
	_panel->setPosition(0, 0);

	_overlay->add2D(_panel);

	_overlay->show();
}

void TextRenderer::addTextBox(const std::string& ID,
	const std::string& text,
	Ogre::Real x, Ogre::Real y,
	Ogre::Real width, Ogre::Real height,
	const Ogre::ColourValue& color)
{
	Ogre::TextAreaOverlayElement* textBox = static_cast<Ogre::TextAreaOverlayElement*>(_overlayMgr->createOverlayElement("TextArea", ID));
	textBox->setDimensions(width, height);
	textBox->setMetricsMode(Ogre::GMM_PIXELS);
	textBox->setPosition(x, y);
	textBox->setFontName("MyFont");
	textBox->setCharHeight(20);
	textBox->setColour(color);

	textBox->setCaption(text);
	textBox->setEnabled(true);

	_panel->addChild(textBox);

	_overlay->show();
}

void TextRenderer::removeTextBox(const std::string& ID)
{
	_panel->removeChild(ID);
	_overlayMgr->destroyOverlayElement(ID);
}

void TextRenderer::setText(const std::string& ID, const std::string& Text)
{
	Ogre::OverlayElement* textBox = _overlayMgr->getOverlayElement(ID);
	textBox->setCaption(Text);
}

const std::string& TextRenderer::getText(const std::string& ID)
{
	Ogre::OverlayElement* textBox = _overlayMgr->getOverlayElement(ID);
	return textBox->getCaption();
}

void TextRenderer::printf(const std::string& ID, const char *fmt)
{
	char        text[256];
	va_list        ap;

	if (fmt == NULL)
		*text = 0;

	else {
		va_start(ap, fmt);
		vsprintf(text, fmt, ap);
		va_end(ap);
	}

	Ogre::OverlayElement* textBox = _overlayMgr->getOverlayElement(ID);
	textBox->setCaption(text);
}