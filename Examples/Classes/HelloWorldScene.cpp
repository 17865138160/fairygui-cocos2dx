#include "HelloWorldScene.h"
#include "SimpleAudioEngine.h"

USING_NS_CC;
USING_NS_FGUI;

Scene* HelloWorld::createScene()
{
    return HelloWorld::create();
}

// Print useful error message instead of segfaulting when files are not there.
static void problemLoading(const char* filename)
{
    printf("Error while loading: %s\n", filename);
    printf("Depending on how you compiled you might have to add 'Resources/' in front of filenames in HelloWorldScene.cpp\n");
}

// on "init" you need to initialize your instance
bool HelloWorld::init()
{
    //////////////////////////////
    // 1. super init first
    if (!Scene::init())
    {
        return false;
    }

    auto visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    /////////////////////////////
    // 2. add a menu item with "X" image, which is clicked to quit the program
    //    you may modify it.

    // add a "close" icon to exit the progress. it's an autorelease object
    auto closeItem = MenuItemImage::create(
        "CloseNormal.png",
        "CloseSelected.png",
        CC_CALLBACK_1(HelloWorld::menuCloseCallback, this));

    if (closeItem == nullptr ||
        closeItem->getContentSize().width <= 0 ||
        closeItem->getContentSize().height <= 0)
    {
        problemLoading("'CloseNormal.png' and 'CloseSelected.png'");
    }
    else
    {
        float x = origin.x + visibleSize.width - closeItem->getContentSize().width / 2;
        float y = origin.y + closeItem->getContentSize().height / 2;
        closeItem->setPosition(Vec2(x, y));
    }

    // create menu, it's an autorelease object
    auto menu = Menu::create(closeItem, NULL);
    menu->setPosition(Vec2::ZERO);
    this->addChild(menu, 1);

    /////////////////////////////
    // 3. add your codes below...

    this->addChild(GRoot::getInstance()->displayObject());

#ifdef CC_PLATFORM_PC
    //cocos2dx��PC����ϵͳ����û����߹��ܣ������ø�ttf������Ϊ����
    UIConfig::registerFont(UIConfig::defaultFont, "fonts/DroidSansFallback.ttf");

    //demo�����õ�һ����������������ı�
    UIConfig::registerFont(u8"΢���ź�", "Microsoft YaHei");
#endif

    UIConfig::buttonSound = "ui://Basics/click";
    UIConfig::verticalScrollBar = "ui://Basics/ScrollBar_VT";
    UIConfig::horizontalScrollBar = "ui://Basics/ScrollBar_HZ";

    UIPackage::addPackage("UI/Basics");
    _view = UIPackage::createObject("Basics", "Main")->asCom();
    GRoot::getInstance()->addChild(_view);

    _backBtn = _view->getChild("btn_Back");
    _backBtn->setVisible(false);
    _backBtn->addClickListener(CC_CALLBACK_1(HelloWorld::onClickBack, this));

    _demoContainer = _view->getChild("container")->asCom();
    _cc = _view->getController("c1");

    int cnt = _view->numChildren();
    for (int i = 0; i < cnt; i++)
    {
        GObject* obj = _view->getChildAt(i);
        if (obj->getGroup() != nullptr && obj->getGroup()->name.compare("btns") == 0)
            obj->addClickListener(CC_CALLBACK_1(HelloWorld::runDemo, this));
    }

    return true;
}

void HelloWorld::onClickBack(EventContext* context)
{
    _cc->setSelectedIndex(0);
    _backBtn->setVisible(false);
}

void HelloWorld::runDemo(EventContext* context)
{
    std::string type = ((GObject*)context->getSender())->name.substr(4);
    auto it = _demoObjects.find(type);
    GComponent* obj;
    if (it == _demoObjects.end())
    {
        obj = UIPackage::createObject("Basics", "Demo_" + type)->asCom();
        _demoObjects.insert(type, obj);
    }
    else
        obj = it->second;

    _demoContainer->removeChildren();
    _demoContainer->addChild(obj);
    _cc->setSelectedIndex(1);
    _backBtn->setVisible(true);

    if (type == "Depth")
        playDepth();
    else if (type == "Window")
        playWindow();
    else if (type == "Drag&Drop")
        playDragDrop();
}

void HelloWorld::playWindow()
{
    GComponent* obj = _demoObjects.at("Window");
    if (_winA == nullptr)
    {
        _winA = Window1::create();
        _winA->retain();

        _winB = Window2::create();
        _winB->retain();

        obj->getChild("n0")->addClickListener([this](EventContext*)
        {
            _winA->show();
        });

        obj->getChild("n1")->addClickListener([this](EventContext*)
        {
            _winB->show();
        });
    }
}

Vec2 startPos;
void HelloWorld::playDepth()
{
    GComponent* obj = _demoObjects.at("Depth");
    GComponent* testContainer = obj->getChild("n22")->asCom();
    GObject* fixedObj = testContainer->getChild("n0");
    fixedObj->setSortingOrder(100);
    fixedObj->setDraggable(true);

    int numChildren = testContainer->numChildren();
    int i = 0;
    while (i < numChildren)
    {
        GObject* child = testContainer->getChildAt(i);
        if (child != fixedObj)
        {
            testContainer->removeChildAt(i);
            numChildren--;
        }
        else
            i++;
    }
    startPos = fixedObj->getPosition();

    obj->getChild("btn0")->addClickListener([obj](EventContext*)
    {
        GGraph* graph = GGraph::create();
        startPos.x += 10;
        startPos.y += 10;
        graph->setPosition(startPos.x, startPos.y);
        graph->drawRect(150, 150, 1, Color4F::BLACK, Color4F::RED);
        obj->getChild("n22")->asCom()->addChild(graph);
    }, (int)this);

    obj->getChild("btn1")->addClickListener([obj](EventContext*)
    {
        GGraph* graph = GGraph::create();
        startPos.x += 10;
        startPos.y += 10;
        graph->setPosition(startPos.x, startPos.y);
        graph->drawRect(150, 150, 1, Color4F::BLACK, Color4F::GREEN);
        graph->setSortingOrder(200);
        obj->getChild("n22")->asCom()->addChild(graph);
    }, (int)this);
}

void HelloWorld::playDragDrop()
{
    GComponent* obj = _demoObjects.at("Drag&Drop");
    obj->getChild("a")->setDraggable(true);

    GButton* b = obj->getChild("b")->asButton();
    b->setDraggable(true);
    b->addEventListener(UIEventType::DragStart, [b](EventContext* context)
    {
        //Cancel the original dragging, and start a new one with a agent.
        context->preventDefault();

        DragDropManager::getInstance()->startDrag(b->getIcon(), (void*)b->getIcon().c_str(),
            context->getInput()->getTouchId());
    });

    GButton* c = obj->getChild("c")->asButton();
    c->setIcon("");
    c->addEventListener(UIEventType::Drop, [c](EventContext* context)
    {
        c->setIcon((char*)context->getData());
    });

    GObject* bounds = obj->getChild("n7");
    Rect rect = bounds->transformRect(Rect(Vec2::ZERO, bounds->getSize()), GRoot::getInstance());

    //---!!Because at this time the container is on the right side of the stage and beginning to move to left(transition), so we need to caculate the final position
    rect.origin.x -= obj->getParent()->getX();;
    //----

    GButton* d = obj->getChild("d")->asButton();
    d->setDraggable(true);
    d->setDragBounds(rect);
}

void HelloWorld::menuCloseCallback(Ref* pSender)
{
    //Close the cocos2d-x game scene and quit the application
    Director::getInstance()->end();

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
    exit(0);
#endif

    /*To navigate back to native iOS screen(if present) without quitting the application  ,do not use Director::getInstance()->end() and exit(0) as given above,instead trigger a custom event created in RootViewController.mm as below*/

    //EventCustom customEndEvent("game_scene_close_event");
    //_eventDispatcher->dispatchEvent(&customEndEvent);


}
