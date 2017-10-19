#include "ScrollPane.h"
#include "display/ScissorClipNode.h"
#include "UIPackage.h"
#include "GList.h"

NS_FGUI_BEGIN
USING_NS_CC;

ScrollPane* ScrollPane::draggingPane = nullptr;
int ScrollPane::_gestureFlag = 0;

const float TWEEN_TIME_GO = 0.5f; //����SetPos(ani)ʱʹ�õĻ���ʱ��
const float TWEEN_TIME_DEFAULT = 0.3f; //���Թ�������С����ʱ��
const float PULL_RATIO = 0.5f; //��������������������ʱ�������ľ���ռ��ʾ����ı���

inline float getPart(const Vec2& pt, int axis) { return axis == 0 ? pt.x : pt.y; }
inline float getPart(const cocos2d::Size& sz, int axis) { return axis == 0 ? sz.width : sz.height; }
void setPart(Vec2& pt, int axis, float value) { if (axis == 0) pt.x = value; else pt.y = value; }
void setPart(cocos2d::Size& sz, int axis, float value) { if (axis == 0) sz.width = value; else sz.height = value; }
void incPart(Vec2& pt, int axis, float value) { if (axis == 0) pt.x += value; else pt.y += value; }
void incPart(cocos2d::Size& sz, int axis, float value) { if (axis == 0) sz.width += value; else sz.height += value; }

inline float EaseFunc(float t, float d)
{
    return (t = t / d - 1) * t * t + 1;//cubicOut
}

ScrollPane::ScrollPane(GComponent* owner,
    ScrollType scrollType,
    const Margin& scrollBarMargin,
    ScrollBarDisplayType scrollBarDisplay,
    int flags,
    const std::string& vtScrollBarRes,
    const std::string& hzScrollBarRes,
    const std::string& headerRes,
    const std::string& footerRes) :
    _vtScrollBar(nullptr),
    _hzScrollBar(nullptr),
    _header(nullptr),
    _footer(nullptr),
    _pageController(nullptr),
    _needRefresh(false),
    _refreshBarAxis(0),
    _aniFlag(0),
    _loop(0),
    _headerLockedSize(0),
    _footerLockedSize(0),
    _xPos(0),
    _yPos(0)
{
    _owner = owner;

    _maskContainer = ScissorClipNode::create();
    _owner->displayObject()->addChild(_maskContainer);

    _container = _owner->getContainer();
    _container->setPosition(0, 0);
    _container->removeFromParent();
    _maskContainer->addChild(_container);

    _scrollBarMargin = scrollBarMargin;
    _scrollType = scrollType;
    _scrollStep = UIConfig::defaultScrollStep;
    _mouseWheelStep = _scrollStep * 2;
    _decelerationRate = UIConfig::defaultScrollDecelerationRate;

    _displayOnLeft = (flags & 1) != 0;
    _snapToItem = (flags & 2) != 0;
    _displayInDemand = (flags & 4) != 0;
    _pageMode = (flags & 8) != 0;
    if ((flags & 16) != 0)
        _touchEffect = true;
    else if ((flags & 32) != 0)
        _touchEffect = false;
    else
        _touchEffect = UIConfig::defaultScrollTouchEffect;
    if ((flags & 64) != 0)
        _bouncebackEffect = true;
    else if ((flags & 128) != 0)
        _bouncebackEffect = false;
    else
        _bouncebackEffect = UIConfig::defaultScrollBounceEffect;
    _inertiaDisabled = (flags & 256) != 0;
    _maskDisabled = (flags & 512) != 0;

    _scrollBarVisible = true;
    _mouseWheelEnabled = true;
    _pageSize = Vec2::ONE;

    if (scrollBarDisplay == ScrollBarDisplayType::SBD_DEFAULT)
    {
        //if (Application.isMobilePlatform)
            //scrollBarDisplay = ScrollBarDisplayType.Auto;
        //else
        scrollBarDisplay = UIConfig::defaultScrollBarDisplay;
    }

    if (scrollBarDisplay != ScrollBarDisplayType::SDB_HIDDEN)
    {
        if (_scrollType == ScrollType::ST_BOTH
            || _scrollType == ScrollType::ST_VERTICAL)
        {
            const std::string& res = vtScrollBarRes.size() == 0 ? UIConfig::verticalScrollBar : vtScrollBarRes;
            if (res.length() > 0)
            {
                _vtScrollBar = dynamic_cast<GScrollBar*>(UIPackage::createObjectFromURL(res));
                if (_vtScrollBar == nullptr)
                    CCLOGWARN("FairyGUI: cannot create scrollbar from %s", res.c_str());
                else
                {
                    _vtScrollBar->retain();
                    _vtScrollBar->setScrollPane(this, true);
                    _owner->displayObject()->addChild(_vtScrollBar->displayObject());
                }
            }
        }
        if (_scrollType == ScrollType::ST_BOTH
            || _scrollType == ScrollType::ST_HORIZONTAL)
        {
            const std::string& res = hzScrollBarRes.length() > 0 ? UIConfig::horizontalScrollBar : hzScrollBarRes;
            if (res.length() > 0)
            {
                _hzScrollBar = dynamic_cast<GScrollBar*>(UIPackage::createObjectFromURL(res));
                if (_hzScrollBar == nullptr)
                    CCLOGWARN("FairyGUI: cannot create scrollbar from %s", res.c_str());
                else
                {
                    _hzScrollBar->retain();
                    _hzScrollBar->setScrollPane(this, false);
                    _owner->displayObject()->addChild(_hzScrollBar->displayObject());
                }
            }
        }

        _scrollBarDisplayAuto = scrollBarDisplay == ScrollBarDisplayType::SBD_AUTO;
        if (_scrollBarDisplayAuto)
        {
            if (_vtScrollBar != nullptr)
                _vtScrollBar->displayObject()->setVisible(false);
            if (_hzScrollBar != nullptr)
                _hzScrollBar->displayObject()->setVisible(false);
            _scrollBarVisible = false;

            _owner->addEventListener(UIEventType::RollOver, CC_CALLBACK_1(ScrollPane::onRollOver, this));
            _owner->addEventListener(UIEventType::RollOut, CC_CALLBACK_1(ScrollPane::onRollOut, this));
        }
    }
    else
        _mouseWheelEnabled = false;

    if (headerRes.length() > 0)
    {
        _header = dynamic_cast<GComponent*>(UIPackage::createObjectFromURL(headerRes));
        if (_header == nullptr)
            CCLOGWARN("FairyGUI: cannot create scrollPane header from %s", headerRes.c_str());
        else
            _header->retain();
    }

    if (footerRes.length() > 0)
    {
        _footer = dynamic_cast<GComponent*>(UIPackage::createObjectFromURL(footerRes));
        if (_footer == nullptr)
            CCLOGWARN("FairyGUI: cannot create scrollPane footer from %s", footerRes.c_str());
        else
            _footer->retain();
    }

    if (_header != nullptr || _footer != nullptr)
        _refreshBarAxis = (_scrollType == ScrollType::ST_BOTH
            || _scrollType == ScrollType::ST_VERTICAL) ? 1 : 0;

    setSize(owner->getWidth(), owner->getHeight());

    _owner->addEventListener(UIEventType::MouseWheel, CC_CALLBACK_1(ScrollPane::onMouseWheel, this));
    _owner->addEventListener(UIEventType::TouchBegin, CC_CALLBACK_1(ScrollPane::onTouchBegin, this));
}

ScrollPane::~ScrollPane()
{
    CALL_PER_FRAME_CANCEL(ScrollPane, tweenUpdate);
    CALL_LATER_CANCEL(ScrollPane, refresh);
    CALL_LATER_CANCEL(ScrollPane, onShowScrollBar);

    CC_SAFE_RELEASE(_hzScrollBar);
    CC_SAFE_RELEASE(_vtScrollBar);
    CC_SAFE_RELEASE(_header);
    CC_SAFE_RELEASE(_footer);
}

void ScrollPane::setScrollStep(float value)
{
    _scrollStep = value;
    if (_scrollStep == 0)
        _scrollStep = UIConfig::defaultScrollStep;
    _mouseWheelStep = _scrollStep * 2;
}

void ScrollPane::setPosX(float value, bool ani)
{
    _owner->ensureBoundsCorrect();

    if (_loop == 1)
        loopCheckingNewPos(value, 0);

    value = clampf(value, 0, _overlapSize.width);
    if (value != _xPos)
    {
        _xPos = value;
        posChanged(ani);
    }
}

void ScrollPane::setPosY(float value, bool ani)
{
    _owner->ensureBoundsCorrect();

    if (_loop == 2)
        loopCheckingNewPos(value, 1);

    value = clampf(value, 0, _overlapSize.height);
    if (value != _yPos)
    {
        _yPos = value;
        posChanged(ani);
    }
}

float ScrollPane::getPercX()
{
    return _overlapSize.width == 0 ? 0 : _xPos / _overlapSize.width;
}

void ScrollPane::setPercX(float value, bool ani)
{
    _owner->ensureBoundsCorrect();
    setPosX(_overlapSize.width * clampf(value, 0, 1), ani);
}

float ScrollPane::getPercY()
{
    return _overlapSize.height == 0 ? 0 : _yPos / _overlapSize.height;
}

void ScrollPane::setPercY(float value, bool ani)
{
    _owner->ensureBoundsCorrect();
    setPosY(_overlapSize.height * clampf(value, 0, 1), ani);
}

bool ScrollPane::isBottomMost()
{
    return _yPos == _overlapSize.height || _overlapSize.height == 0;
}

bool ScrollPane::isRightMost()
{
    return _xPos == _overlapSize.width || _overlapSize.width == 0;
}

void ScrollPane::scrollLeft(float ratio, bool ani)
{
    if (_pageMode)
        setPosX(_xPos - _pageSize.width * ratio, ani);
    else
        setPosX(_xPos - _scrollStep * ratio, ani);
}

void ScrollPane::scrollRight(float ratio, bool ani)
{
    if (_pageMode)
        setPosX(_xPos + _pageSize.width * ratio, ani);
    else
        setPosX(_xPos + _scrollStep * ratio, ani);
}

void ScrollPane::scrollUp(float ratio, bool ani)
{
    if (_pageMode)
        setPosY(_yPos - _pageSize.height * ratio, ani);
    else
        setPosY(_yPos - _scrollStep * ratio, ani);
}

void ScrollPane::scrollDown(float ratio, bool ani)
{
    if (_pageMode)
        setPosY(_yPos + _pageSize.height * ratio, ani);
    else
        setPosY(_yPos + _scrollStep * ratio, ani);
}

void ScrollPane::scrollTop(bool ani)
{
    setPercY(0, ani);
}

void ScrollPane::scrollBottom(bool ani)
{
    setPercY(1, ani);
}

void ScrollPane::scrollToView(GObject * obj, bool ani, bool setFirst)
{
    _owner->ensureBoundsCorrect();
    if (_needRefresh)
        refresh();

    Rect rect = Rect(obj->getX(), obj->getY(), obj->getWidth(), obj->getHeight());
    //if (obj->getParent() != _owner)
    //    rect = obj->getParent()->transformRect(rect, _owner);
    scrollToView(rect, ani, setFirst);
}

void ScrollPane::scrollToView(const cocos2d::Rect & rect, bool ani, bool setFirst)
{
    _owner->ensureBoundsCorrect();
    if (_needRefresh)
        refresh();

    if (_overlapSize.height > 0)
    {
        float bottom = _yPos + _viewSize.height;
        if (setFirst || rect.origin.y <= _yPos || rect.size.height >= _viewSize.height)
        {
            if (_pageMode)
                setPosY(floor(rect.origin.y / _pageSize.height) * _pageSize.height, ani);
            else
                setPosY(rect.origin.y, ani);
        }
        else if (rect.getMaxY() > bottom)
        {
            if (_pageMode)
                setPosY(floor(rect.origin.y / _pageSize.height) * _pageSize.height, ani);
            else if (rect.size.height <= _viewSize.height / 2)
                setPosY(rect.origin.y + rect.size.height * 2 - _viewSize.height, ani);
            else
                setPosY(rect.getMaxY() - _viewSize.height, ani);
        }
    }
    if (_overlapSize.width > 0)
    {
        float right = _xPos + _viewSize.width;
        if (setFirst || rect.origin.x <= _xPos || rect.size.width >= _viewSize.width)
        {
            if (_pageMode)
                setPosX(floor(rect.origin.x / _pageSize.width) * _pageSize.width, ani);
            setPosX(rect.origin.x, ani);
        }
        else if (rect.getMaxX() > right)
        {
            if (_pageMode)
                setPosX(floor(rect.origin.x / _pageSize.width) * _pageSize.width, ani);
            else if (rect.size.width <= _viewSize.width / 2)
                setPosX(rect.origin.x + rect.size.width * 2 - _viewSize.width, ani);
            else
                setPosX(rect.getMaxX() - _viewSize.width, ani);
        }
    }

    if (!ani && _needRefresh)
        refresh();
}

bool ScrollPane::isChildInView(GObject * obj)
{
    if (_overlapSize.height > 0)
    {
        float dist = obj->getY() + _container->getPositionY();
        if (dist < -obj->getHeight() - 20 || dist > _viewSize.height + 20)
            return false;
    }
    if (_overlapSize.width > 0)
    {
        float dist = obj->getX() + _container->getPositionX();
        if (dist < -obj->getWidth() - 20 || dist > _viewSize.width + 20)
            return false;
    }

    return true;
}

int ScrollPane::getPageX()
{
    if (!_pageMode)
        return 0;

    int page = floor(_xPos / _pageSize.width);
    if (_xPos - page * _pageSize.width > _pageSize.width * 0.5f)
        page++;

    return page;
}

void ScrollPane::setPageX(int value, bool ani)
{
    if (!_pageMode)
        return;

    if (_overlapSize.width > 0)
        setPosX(value * _pageSize.width, ani);
}

int ScrollPane::getPageY()
{
    if (!_pageMode)
        return 0;

    int page = floor(_yPos / _pageSize.height);
    if (_yPos - page * _pageSize.height > _pageSize.height * 0.5f)
        page++;

    return page;
}

void ScrollPane::setPageY(int value, bool ani)
{
    if (_overlapSize.height > 0)
        setPosY(value * _pageSize.height, ani);
}

float ScrollPane::getScrollingPosX()
{
    return clampf(-_container->getPositionX(), 0, _overlapSize.width);
}

float ScrollPane::getScrollingPosY()
{
    return clampf(-_container->getPositionY(), 0, _overlapSize.height);
}

void ScrollPane::setViewWidth(float value)
{
    value = value + _owner->getMargin().left + _owner->getMargin().right;
    if (_vtScrollBar != nullptr)
        value += _vtScrollBar->getWidth();
    _owner->setWidth(value);
}

void ScrollPane::setViewHeight(float value)
{
    value = value + _owner->getMargin().top + _owner->getMargin().bottom;
    if (_hzScrollBar != nullptr)
        value += _hzScrollBar->getHeight();
    _owner->setHeight(value);
}

void ScrollPane::lockHeader(int size)
{
    if (_headerLockedSize == size)
        return;

    _headerLockedSize = size;
    if (!_owner->isDispatchingEvent(UIEventType::PullDownRelease)
        && getPart(_container->getPosition(), _refreshBarAxis) >= 0)
    {
        _tweenStart = _container->getPosition();
        _tweenChange.setZero();
        setPart(_tweenChange, _refreshBarAxis, _headerLockedSize - getPart(_tweenStart, _refreshBarAxis));
        _tweenDuration.set(TWEEN_TIME_DEFAULT, TWEEN_TIME_DEFAULT);
        _tweenTime.setZero();
        _tweening = 2;

        CALL_PER_FRAME(ScrollPane, tweenUpdate);
    }
}

void ScrollPane::lockFooter(int size)
{
    if (_footerLockedSize == size)
        return;

    _footerLockedSize = size;
    if (!_owner->isDispatchingEvent(UIEventType::PullUpRelease)
        && getPart(_container->getPosition(), _refreshBarAxis) >= 0)
    {
        _tweenStart = _container->getPosition();
        _tweenChange.setZero();
        float max = getPart(_overlapSize, _refreshBarAxis);
        if (max == 0)
            max = MAX(getPart(_contentSize, _refreshBarAxis) + _footerLockedSize - getPart(_viewSize, _refreshBarAxis), 0);
        else
            max += _footerLockedSize;
        setPart(_tweenChange, _refreshBarAxis, -max - getPart(_tweenStart, _refreshBarAxis));
        _tweenDuration.set(TWEEN_TIME_DEFAULT, TWEEN_TIME_DEFAULT);
        _tweenTime.setZero();
        _tweening = 2;

        CALL_PER_FRAME(ScrollPane, tweenUpdate);
    }
}

void ScrollPane::cancelDragging()
{
    if (draggingPane == this)
        draggingPane = nullptr;

    _gestureFlag = 0;
    _isMouseMoved = false;
}

void ScrollPane::onOwnerSizeChanged()
{
    setSize(_owner->getWidth(), _owner->getHeight());
    posChanged(false);
}

void ScrollPane::handleControllerChanged(Controller * c)
{
    if (_pageController == c)
    {
        if (_scrollType == ScrollType::ST_HORIZONTAL)
            setPageX(c->getSelectedIndex());
        else
            setPageY(c->getSelectedIndex());
    }
}

void ScrollPane::updatePageController()
{
    if (_pageController != nullptr && !_pageController->changing)
    {
        int index;
        if (_scrollType == ScrollType::ST_HORIZONTAL)
            index = getPageX();
        else
            index = getPageY();
        if (index < _pageController->getPageCount())
        {
            Controller* c = _pageController;
            _pageController = nullptr; //��ֹHandleControllerChanged�ĵ���
            c->setSelectedIndex(index);
            _pageController = c;
        }
    }
}

void ScrollPane::adjustMaskContainer()
{
    float mx, my;
    if (_displayOnLeft && _vtScrollBar != nullptr)
        mx = floor(_owner->getMargin().left + _vtScrollBar->getWidth());
    else
        mx = floor(_owner->getMargin().left);
    my = floor(_owner->getMargin().top);
    mx += _owner->getAlignOffset().x;
    my += _owner->getAlignOffset().y;

    _maskContainer->setPosition(Vec2(mx, my));
}

void ScrollPane::setSize(float aWidth, float aHeight)
{
    adjustMaskContainer();

    if (_hzScrollBar != nullptr)
    {
        _hzScrollBar->setY(aHeight - _hzScrollBar->getHeight());
        if (_vtScrollBar != nullptr)
        {
            _hzScrollBar->setWidth(aWidth - _vtScrollBar->getWidth() - _scrollBarMargin.left - _scrollBarMargin.right);
            if (_displayOnLeft)
                _hzScrollBar->setX(_scrollBarMargin.left + _vtScrollBar->getWidth());
            else
                _hzScrollBar->setX(_scrollBarMargin.left);
        }
        else
        {
            _hzScrollBar->setWidth(aWidth - _scrollBarMargin.left - _scrollBarMargin.right);
            _hzScrollBar->setX(_scrollBarMargin.left);
        }
    }
    if (_vtScrollBar != nullptr)
    {
        if (!_displayOnLeft)
            _vtScrollBar->setX(aWidth - _vtScrollBar->getWidth());
        if (_hzScrollBar != nullptr)
            _vtScrollBar->setHeight(aHeight - _hzScrollBar->getHeight() - _scrollBarMargin.top - _scrollBarMargin.bottom);
        else
            _vtScrollBar->setHeight(aHeight - _scrollBarMargin.top - _scrollBarMargin.bottom);
        _vtScrollBar->setY(_scrollBarMargin.top);
    }

    _viewSize.width = aWidth;
    _viewSize.height = aHeight;
    if (_hzScrollBar != nullptr && !_hScrollNone)
        _viewSize.height -= _hzScrollBar->getHeight();
    if (_vtScrollBar != nullptr && !_vScrollNone)
        _viewSize.width -= _vtScrollBar->getWidth();
    _viewSize.width -= (_owner->getMargin().left + _owner->getMargin().right);
    _viewSize.height -= (_owner->getMargin().top + _owner->getMargin().bottom);

    _viewSize.width = MAX(1, _viewSize.width);
    _viewSize.height = MAX(1, _viewSize.height);
    _pageSize = _viewSize;

    handleSizeChanged();
}

void ScrollPane::setContentSize(float aWidth, float aHeight)
{
    if (_contentSize.width == aWidth && _contentSize.height == aHeight)
        return;

    _contentSize.width = aWidth;
    _contentSize.height = aHeight;
    handleSizeChanged();
}

void ScrollPane::changeContentSizeOnScrolling(float deltaWidth, float deltaHeight, float deltaPosX, float deltaPosY)
{
    bool isRightmost = _xPos == _overlapSize.width;
    bool isBottom = _yPos == _overlapSize.height;

    _contentSize.width += deltaWidth;
    _contentSize.height += deltaHeight;
    handleSizeChanged();

    if (_tweening == 1)
    {
        //���ԭ������λ�������ߣ����봦��������ߡ�
        if (deltaWidth != 0 && isRightmost && _tweenChange.x < 0)
        {
            _xPos = _overlapSize.width;
            _tweenChange.x = -_xPos - _tweenStart.x;
        }

        if (deltaHeight != 0 && isBottom && _tweenChange.y < 0)
        {
            _yPos = _overlapSize.height;
            _tweenChange.y = -_yPos - _tweenStart.y;
        }
    }
    else if (_tweening == 2)
    {
        //���µ�����ʼλ�ã�ȷ���ܹ�˳������ȥ
        if (deltaPosX != 0)
        {
            _container->setPositionX(_container->getPositionX() - deltaPosX);
            _tweenStart.x -= deltaPosX;
            _xPos = -_container->getPositionX();
        }
        if (deltaPosY != 0)
        {
            _container->setPositionY(_container->getPositionY() - deltaPosY);
            _tweenStart.y -= deltaPosY;
            _yPos = -_container->getPositionY();
        }
    }
    else if (_isMouseMoved)
    {
        if (deltaPosX != 0)
        {
            _container->setPositionX(_container->getPositionX() - deltaPosX);
            _containerPos.x -= deltaPosX;
            _xPos = -_container->getPositionX();
        }
        if (deltaPosY != 0)
        {
            _container->setPositionY(_container->getPositionY() - deltaPosY);
            _containerPos.y -= deltaPosY;
            _yPos = -_container->getPositionY();
        }
    }
    else
    {
        //���ԭ������λ�������ߣ����봦��������ߡ�
        if (deltaWidth != 0 && isRightmost)
        {
            _xPos = _overlapSize.width;
            _container->setPositionX(_container->getPositionX() - _xPos);
        }

        if (deltaHeight != 0 && isBottom)
        {
            _yPos = _overlapSize.height;
            _container->setPositionY(_container->getPositionY() - _yPos);
        }
    }

    if (_pageMode)
        updatePageController();
}

void ScrollPane::handleSizeChanged()
{
    if (_displayInDemand)
    {
        if (_vtScrollBar != nullptr)
        {
            if (_contentSize.height <= _viewSize.height)
            {
                if (!_vScrollNone)
                {
                    _vScrollNone = true;
                    _viewSize.width += _vtScrollBar->getWidth();
                }
            }
            else
            {
                if (_vScrollNone)
                {
                    _vScrollNone = false;
                    _viewSize.width -= _vtScrollBar->getWidth();
                }
            }
        }
        if (_hzScrollBar != nullptr)
        {
            if (_contentSize.width <= _viewSize.width)
            {
                if (!_hScrollNone)
                {
                    _hScrollNone = true;
                    _viewSize.height += _hzScrollBar->getHeight();
                }
            }
            else
            {
                if (_hScrollNone)
                {
                    _hScrollNone = false;
                    _viewSize.height -= _hzScrollBar->getHeight();
                }
            }
        }
    }

    if (_vtScrollBar != nullptr)
    {
        if (_viewSize.height < _vtScrollBar->getMinSize())
            _vtScrollBar->displayObject()->setVisible(false);
        else
        {
            _vtScrollBar->displayObject()->setVisible(_scrollBarVisible && !_vScrollNone);
            if (_contentSize.height == 0)
                _vtScrollBar->setDisplayPerc(0);
            else
                _vtScrollBar->setDisplayPerc(MIN(1, _viewSize.height / _contentSize.height));
        }
    }
    if (_hzScrollBar != nullptr)
    {
        if (_viewSize.width < _hzScrollBar->getMinSize())
            _hzScrollBar->displayObject()->setVisible(false);
        else
        {
            _hzScrollBar->displayObject()->setVisible(_scrollBarVisible && !_hScrollNone);
            if (_contentSize.width == 0)
                _hzScrollBar->setDisplayPerc(0);
            else
                _hzScrollBar->setDisplayPerc(MIN(1, _viewSize.width / _contentSize.width));
        }
    }

    if (!_maskDisabled)
        _maskContainer->setContentSize(_viewSize);

    if (_scrollType == ScrollType::ST_HORIZONTAL || _scrollType == ScrollType::ST_BOTH)
        _overlapSize.width = ceil(MAX(0, _contentSize.width - _viewSize.width));
    else
        _overlapSize.width = 0;
    if (_scrollType == ScrollType::ST_VERTICAL || _scrollType == ScrollType::ST_BOTH)
        _overlapSize.height = ceil(MAX(0, _contentSize.height - _viewSize.height));
    else
        _overlapSize.height = 0;

    //�߽���
    _xPos = clampf(_xPos, 0, _overlapSize.width);
    _yPos = clampf(_yPos, 0, _overlapSize.height);
    float max = getPart(_overlapSize, _refreshBarAxis);
    if (max == 0)
        max = MAX(getPart(_contentSize, _refreshBarAxis) + _footerLockedSize - getPart(_viewSize, _refreshBarAxis), 0);
    else
        max += _footerLockedSize;
    if (_refreshBarAxis == 0)
        _container->setPosition(clampf(_container->getPositionX(), -max, _headerLockedSize),
            clampf(_container->getPositionY(), -_overlapSize.height, 0));
    else
        _container->setPosition(clampf(_container->getPositionX(), -_overlapSize.width, 0),
            clampf(_container->getPositionY(), -max, _headerLockedSize));

    if (_header != nullptr)
    {
        if (_refreshBarAxis == 0)
            _header->setHeight(_viewSize.height);
        else
            _header->setWidth(_viewSize.width);
    }

    if (_footer != nullptr)
    {
        if (_refreshBarAxis == 0)
            _footer->setHeight(_viewSize.height);
        else
            _footer->setWidth(_viewSize.width);
    }

    syncScrollBar();
    checkRefreshBar();
    if (_pageMode)
        updatePageController();
}

void ScrollPane::posChanged(bool ani)
{
    //ֻҪ��1��Ҫ��Ҫ�������ǾͲ�����
    if (_aniFlag == 0)
        _aniFlag = ani ? 1 : -1;
    else if (_aniFlag == 1 && !ani)
        _aniFlag = -1;

    _needRefresh = true;
    CALL_LATER(ScrollPane, refresh);
}

void ScrollPane::refresh()
{
    CALL_LATER_CANCEL(ScrollPane, refresh);
    _needRefresh = false;

    if (_pageMode || _snapToItem)
    {
        Vec2 pos(-_xPos, -_yPos);
        alignPosition(pos, false);
        _xPos = -pos.x;
        _yPos = -pos.y;
    }

    refresh2();

    _owner->dispatchEvent(UIEventType::Scroll);
    if (_needRefresh) //��onScroll�¼��￪���߿����޸�λ�ã�������ˢ��һ�Σ�������˸
    {
        _needRefresh = false;
        CALL_LATER_CANCEL(ScrollPane, refresh);

        refresh2();
    }

    syncScrollBar();
    _aniFlag = 0;
}

void ScrollPane::refresh2()
{
    if (_aniFlag == 1 && !_isMouseMoved)
    {
        Vec2 pos;

        if (_overlapSize.width > 0)
            pos.x = -(int)_xPos;
        else
        {
            if (_container->getPositionX() != 0)
                _container->setPositionX(0);
            pos.x = 0;
        }
        if (_overlapSize.height > 0)
            pos.y = -(int)_yPos;
        else
        {
            if (_container->getPositionY() != 0)
                _container->setPositionY(0);
            pos.y = 0;
        }

        if (pos.x != _container->getPositionX() || pos.y != _container->getPositionY())
        {
            _tweening = 1;
            _tweenTime.setZero();
            _tweenDuration.set(TWEEN_TIME_GO, TWEEN_TIME_GO);
            _tweenStart = _container->getPosition();
            _tweenChange = pos - _tweenStart;
            CALL_PER_FRAME(ScrollPane, tweenUpdate);
        }
        else if (_tweening != 0)
            killTween();
    }
    else
    {
        if (_tweening != 0)
            killTween();

        _container->setPosition(Vec2((int)-_xPos, (int)-_yPos));

        loopCheckingCurrent();
    }

    if (_pageMode)
        updatePageController();
}

void ScrollPane::syncScrollBar(bool end)
{
    if (_vtScrollBar != nullptr)
    {
        _vtScrollBar->setScrollPerc(_overlapSize.height == 0 ? 0 : clampf(-_container->getPositionY(), 0, _overlapSize.height) / _overlapSize.height);
        if (_scrollBarDisplayAuto)
            showScrollBar(!end);
    }
    if (_hzScrollBar != nullptr)
    {
        _hzScrollBar->setScrollPerc(_overlapSize.width == 0 ? 0 : clampf(-_container->getPositionX(), 0, _overlapSize.width) / _overlapSize.width);
        if (_scrollBarDisplayAuto)
            showScrollBar(!end);
    }
}

void ScrollPane::showScrollBar(bool show)
{
    _scrollBarVisible = (bool)show && _viewSize.width > 0 && _viewSize.height > 0;

    if (show)
    {
        onShowScrollBar();
        CALL_LATER_CANCEL(ScrollPane, onShowScrollBar);
    }
    else
        CALL_LATER(ScrollPane, onShowScrollBar, 0.5f);
}

void ScrollPane::onShowScrollBar()
{
    if (_vtScrollBar != nullptr)
        _vtScrollBar->displayObject()->setVisible(_scrollBarVisible && !_vScrollNone);
    if (_hzScrollBar != nullptr)
        _vtScrollBar->displayObject()->setVisible(_scrollBarVisible && !_hScrollNone);
}

float ScrollPane::getLoopPartSize(float division, int axis)
{
    return (getPart(_contentSize, axis) + (axis == 0 ? ((GList*)_owner)->getColumnGap() : ((GList*)_owner)->getLineGap())) / division;
}

bool ScrollPane::loopCheckingCurrent()
{
    bool changed = false;
    if (_loop == 1 && _overlapSize.width > 0)
    {
        if (_xPos < 0.001f)
        {
            _xPos += getLoopPartSize(2, 0);
            changed = true;
        }
        else if (_xPos >= _overlapSize.width)
        {
            _xPos -= getLoopPartSize(2, 0);
            changed = true;
        }
    }
    else if (_loop == 2 && _overlapSize.height > 0)
    {
        if (_yPos < 0.001f)
        {
            _yPos += getLoopPartSize(2, 1);
            changed = true;
        }
        else if (_yPos >= _overlapSize.height)
        {
            _yPos -= getLoopPartSize(2, 1);
            changed = true;
        }
    }

    if (changed)
        _container->setPosition(Vec2((int)-_xPos, (int)-_yPos));

    return changed;
}

void ScrollPane::loopCheckingTarget(cocos2d::Vec2 & endPos)
{
    if (_loop == 1)
        loopCheckingTarget(endPos, 0);

    if (_loop == 2)
        loopCheckingTarget(endPos, 1);
}

void ScrollPane::loopCheckingTarget(cocos2d::Vec2 & endPos, int axis)
{
    if (getPart(endPos, axis) > 0)
    {
        float halfSize = getLoopPartSize(2, axis);
        float tmp = getPart(_tweenStart, axis) - halfSize;
        if (tmp <= 0 && tmp >= -getPart(_overlapSize, axis))
        {
            incPart(endPos, axis, -halfSize);
            setPart(_tweenStart, axis, tmp);
        }
    }
    else if (getPart(endPos, axis) < -getPart(_overlapSize, axis))
    {
        float halfSize = getLoopPartSize(2, axis);
        float tmp = getPart(_tweenStart, axis) + halfSize;
        if (tmp <= 0 && tmp >= -getPart(_overlapSize, axis))
        {
            incPart(endPos, axis, halfSize);
            setPart(_tweenStart, axis, tmp);
        }
    }
}

void ScrollPane::loopCheckingNewPos(float & value, int axis)
{
    float overlapSize = getPart(_overlapSize, axis);
    if (overlapSize == 0)
        return;

    float pos = axis == 0 ? _xPos : _yPos;
    bool changed = false;
    if (value < 0.001f)
    {
        value += getLoopPartSize(2, axis);
        if (value > pos)
        {
            float v = getLoopPartSize(6, axis);
            v = ceil((value - pos) / v) * v;
            pos = clampf(pos + v, 0, overlapSize);
            changed = true;
        }
    }
    else if (value >= overlapSize)
    {
        value -= getLoopPartSize(2, axis);
        if (value < pos)
        {
            float v = getLoopPartSize(6, axis);
            v = ceil((pos - value) / v) * v;
            pos = clampf(pos - v, 0, overlapSize);
            changed = true;
        }
    }

    if (changed)
    {
        if (axis == 0)
            _container->setPositionX(-(int)pos);
        else
            _container->setPositionY(-(int)pos);
    }
}

void ScrollPane::alignPosition(Vec2 & pos, bool inertialScrolling)
{
    if (_pageMode)
    {
        pos.x = alignByPage(pos.x, 0, inertialScrolling);
        pos.y = alignByPage(pos.y, 1, inertialScrolling);
    }
    else if (_snapToItem)
    {
        Vec2 tmp = _owner->getSnappingPosition(-pos);
        if (pos.x < 0 && pos.x > -_overlapSize.width)
            pos.x = -tmp.x;
        if (pos.y < 0 && pos.y > -_overlapSize.height)
            pos.y = -tmp.y;
    }
}

float ScrollPane::alignByPage(float pos, int axis, bool inertialScrolling)
{
    int page;
    float pageSize = getPart(_pageSize, axis);
    float overlapSize = getPart(_overlapSize, axis);
    float contentSize = getPart(_contentSize, axis);

    if (pos > 0)
        page = 0;
    else if (pos < -overlapSize)
        page = ceil(contentSize / pageSize) - 1;
    else
    {
        page = floor(-pos / pageSize);
        float change = inertialScrolling ? (pos - getPart(_containerPos, axis)) : (pos - getPart(_container->getPosition(), axis));
        float testPageSize = MIN(pageSize, contentSize - (page + 1) * pageSize);
        float delta = -pos - page * pageSize;

        //ҳ����������
        if (abs(change) > pageSize)//����������볬��1ҳ,����Ҫ����ҳ���һ�룬���ܵ�����һҳ
        {
            if (delta > testPageSize * 0.5f)
                page++;
        }
        else //����ֻ��Ҫҳ���1/3����Ȼ����Ҫ���ǵ����ƺ����Ƶ����
        {
            if (delta > testPageSize * (change < 0 ? 0.3f : 0.7f))
                page++;
        }

        //���¼����յ�
        pos = -page * pageSize;
        if (pos < -overlapSize) //���һҳδ����pageSize��ô��
            pos = -overlapSize;
    }

    //���Թ���ģʽ�£��������жϾ�����Ҫ��������һҳ
    if (inertialScrolling)
    {
        float oldPos = getPart(_tweenStart, axis);
        int oldPage;
        if (oldPos > 0)
            oldPage = 0;
        else if (oldPos < -overlapSize)
            oldPage = ceil(contentSize / pageSize) - 1;
        else
            oldPage = floor(-oldPos / overlapSize);
        int startPage = floor(-getPart(_containerPos, axis) / overlapSize);
        if (abs(page - startPage) > 1 && abs(oldPage - startPage) <= 1)
        {
            if (page > startPage)
                page = startPage + 1;
            else
                page = startPage - 1;
            pos = -page * overlapSize;
        }
    }

    return pos;
}

cocos2d::Vec2 ScrollPane::updateTargetAndDuration(const cocos2d::Vec2 & orignPos)
{
    Vec2 ret(0, 0);
    ret.x = updateTargetAndDuration(orignPos.x, 0);
    ret.y = updateTargetAndDuration(orignPos.y, 1);
    return ret;
}

float ScrollPane::updateTargetAndDuration(float pos, int axis)
{
    float v = getPart(_velocity, axis);
    float duration = 0;

    if (pos > 0)
        pos = 0;
    else if (pos < -getPart(_overlapSize, axis))
        pos = -getPart(_overlapSize, axis);
    else
    {
        //����Ļ����Ϊ��׼
        float v2 = abs(v) * _velocityScale;
        //���ƶ��豸�ϣ���Ҫ�Բ�ͬ�ֱ�����һ�����䣬���ǵ��ٶ��ж���1136�ֱ���Ϊ��׼
        //if (Stage.touchScreen)
        {
            const cocos2d::Size& winSize = Director::getInstance()->getWinSizeInPixels();
            v2 *= 1136.0f / MAX(winSize.width, winSize.height);
        }
        //������һЩ��ֵ�Ĵ�����Ϊ�ڵ����ڣ���ϣ�������ϴ�Ĺ�����������������
        float ratio = 0;
        if (_pageMode/* || !Stage.touchScreen*/)
        {
            if (v2 > 500)
                ratio = pow((v2 - 500) / 500, 2);
        }
        else
        {
            if (v2 > 1000)
                ratio = pow((v2 - 1000) / 1000, 2);
        }

        if (ratio != 0)
        {
            if (ratio > 1)
                ratio = 1;

            v2 *= ratio;
            v *= ratio;
            setPart(_velocity, axis, v);

            //�㷨��v*��_decelerationRate��n���ݣ�= 60������n֡���ٶȽ�Ϊ60������ÿ��60֡����
            duration = log(60 / v2) / log(_decelerationRate) / 60;

            //�������Ҫʹ�ñ����ٶ�
            //���۹�ʽò�ƹ����ľ��벻������Ϊ���鹫ʽ
            //float change = (int)((v/ 60 - 1) / (1 - _decelerationRate));
            float change = (int)(v * duration * 0.4f);
            pos += change;
        }
    }

    if (duration < TWEEN_TIME_DEFAULT)
        duration = TWEEN_TIME_DEFAULT;
    setPart(_tweenDuration, axis, duration);

    return pos;
}

void ScrollPane::fixDuration(int axis, float oldChange)
{
    float tweenChange = getPart(_tweenChange, axis);
    if (tweenChange == 0 || abs(tweenChange) >= abs(oldChange))
        return;

    float newDuration = abs(tweenChange / oldChange) * getPart(_tweenDuration, axis);
    if (newDuration < TWEEN_TIME_DEFAULT)
        newDuration = TWEEN_TIME_DEFAULT;

    setPart(_tweenDuration, axis, newDuration);
}

void ScrollPane::killTween()
{
    if (_tweening == 1) //ȡ������Ϊ1��tween���������õ��յ�
    {
        _container->setPosition(_tweenStart + _tweenChange);
        _owner->dispatchEvent(UIEventType::Scroll);
    }

    _tweening = 0;
    CALL_PER_FRAME_CANCEL(ScrollPane, tweenUpdate);
    _owner->dispatchEvent(UIEventType::ScrollEnd);
}

void ScrollPane::checkRefreshBar()
{
    if (_header == nullptr && _footer == nullptr)
        return;

    float pos = getPart(_container->getPosition(), _refreshBarAxis);
    if (_header != nullptr)
    {
        if (pos > 0)
        {
            if (_header->displayObject()->getParent() == nullptr)
                _maskContainer->addChild(_header->displayObject(), 0);
            Vec2 vec;

            vec = _header->getSize();
            setPart(vec, _refreshBarAxis, pos);
            _header->setSize(vec.x, vec.y);
        }
        else
        {
            if (_header->displayObject()->getParent() != nullptr)
                _maskContainer->removeChild(_header->displayObject());
        }
    }

    if (_footer != nullptr)
    {
        float max = getPart(_overlapSize, _refreshBarAxis);
        if (pos < -max || max == 0 && _footerLockedSize > 0)
        {
            if (_footer->displayObject()->getParent() == nullptr)
                _maskContainer->addChild(_footer->displayObject(), 0);

            Vec2 vec;

            vec = _footer->getPosition();
            if (max > 0)
                setPart(vec, _refreshBarAxis, pos + getPart(_contentSize, _refreshBarAxis));
            else
                setPart(vec, _refreshBarAxis, MAX(MIN(pos + getPart(_viewSize, _refreshBarAxis),
                    getPart(_viewSize, _refreshBarAxis) - _footerLockedSize),
                    getPart(_viewSize, _refreshBarAxis) - getPart(_contentSize, _refreshBarAxis)));
            _footer->setPosition(vec.x, vec.y);

            vec = _footer->getSize();
            if (max > 0)
                setPart(vec, _refreshBarAxis, -max - pos);
            else
                setPart(vec, _refreshBarAxis, getPart(_viewSize, _refreshBarAxis) - getPart(_footer->getPosition(), _refreshBarAxis));
            _footer->setSize(vec.x, vec.y);
        }
        else
        {
            if (_footer->displayObject()->getParent() != nullptr)
                _maskContainer->removeChild(_footer->displayObject());
        }
    }
}

void ScrollPane::tweenUpdate(float dt)
{
    float nx = runTween(0, dt);
    float ny = runTween(1, dt);

    _container->setPosition(nx, ny);

    if (_tweening == 2)
    {
        if (_overlapSize.width > 0)
            _xPos = clampf(-nx, 0, _overlapSize.width);
        if (_overlapSize.height > 0)
            _yPos = clampf(-ny, 0, _overlapSize.height);

        if (_pageMode)
            updatePageController();
    }

    if (_tweenChange.x == 0 && _tweenChange.y == 0)
    {
        _tweening = 0;
        CALL_PER_FRAME_CANCEL(ScrollPane, tweenUpdate);

        loopCheckingCurrent();

        syncScrollBar(true);
        checkRefreshBar();
        _owner->dispatchEvent(UIEventType::Scroll);
        _owner->dispatchEvent(UIEventType::ScrollEnd);
    }
    else
    {
        syncScrollBar(false);
        checkRefreshBar();
        _owner->dispatchEvent(UIEventType::Scroll);
    }
}

float ScrollPane::runTween(int axis, float dt)
{
    float newValue;
    if (getPart(_tweenChange, axis) != 0)
    {
        incPart(_tweenTime, axis, dt);
        if (getPart(_tweenTime, axis) >= getPart(_tweenDuration, axis))
        {
            newValue = getPart(_tweenStart, axis) + getPart(_tweenChange, axis);
            setPart(_tweenChange, axis, 0);
        }
        else
        {
            float ratio = EaseFunc(getPart(_tweenTime, axis), getPart(_tweenDuration, axis));
            newValue = getPart(_tweenStart, axis) + (int)(getPart(_tweenChange, axis) * ratio);
        }

        float threshold1 = 0;
        float threshold2 = -getPart(_overlapSize, axis);
        if (_headerLockedSize > 0 && _refreshBarAxis == axis)
            threshold1 = _headerLockedSize;
        if (_footerLockedSize > 0 && _refreshBarAxis == axis)
        {
            float max = getPart(_overlapSize, _refreshBarAxis);
            if (max == 0)
                max = MAX(getPart(_contentSize, _refreshBarAxis) + _footerLockedSize - getPart(_viewSize, _refreshBarAxis), 0);
            else
                max += _footerLockedSize;
            threshold2 = -max;
        }

        if (_tweening == 2 && _bouncebackEffect)
        {
            if (newValue > 20 + threshold1 && getPart(_tweenChange, axis) > 0
                || newValue > threshold1 && getPart(_tweenChange, axis) == 0)//��ʼ�ص�
            {
                setPart(_tweenTime, axis, 0);
                setPart(_tweenDuration, axis, TWEEN_TIME_DEFAULT);
                setPart(_tweenChange, axis, -newValue + threshold1);
                setPart(_tweenStart, axis, newValue);
            }
            else if (newValue < threshold2 - 20 && getPart(_tweenChange, axis) < 0
                || newValue < threshold2 && getPart(_tweenChange, axis) == 0)//��ʼ�ص�
            {
                setPart(_tweenTime, axis, 0);
                setPart(_tweenDuration, axis, TWEEN_TIME_DEFAULT);
                setPart(_tweenChange, axis, threshold2 - newValue);
                setPart(_tweenStart, axis, newValue);
            }
        }
        else
        {
            if (newValue > threshold1)
            {
                newValue = threshold1;
                setPart(_tweenChange, axis, 0);
            }
            else if (newValue < threshold2)
            {
                newValue = threshold2;
                setPart(_tweenChange, axis, 0);
            }
        }
    }
    else
        newValue = getPart(_container->getPosition(), axis);

    return newValue;
}

void ScrollPane::onTouchBegin(EventContext * context)
{
    if (!_touchEffect)
        return;

    context->captureTouch();
    InputEvent* evt = context->getInput();
    Vec2 pt = _owner->globalToLocal(evt->getPosition());

    if (_tweening != 0)
    {
        killTween();
        //Stage.inst.CancelClick(evt->getTouchId());

        //����ֹͣ���Թ���������λ�ò����룬�趨�����־��ʹtouchEndʱ��λ
        _isMouseMoved = true;
    }
    else
        _isMouseMoved = false;

    _containerPos = _container->getPosition();
    _beginTouchPos = _lastTouchPos = pt;
    _lastTouchGlobalPos = evt->getPosition();
    _isHoldAreaDone = false;
    _velocity.setZero();
    _velocityScale = 1;
    _lastMoveTime = clock();
}

void ScrollPane::onTouchMove(EventContext * context)
{
    if (!_touchEffect)
        return;

    if (draggingPane != nullptr && draggingPane != this || GObject::draggingObject != nullptr) //�Ѿ��������϶�
        return;

    InputEvent* evt = context->getInput();
    Vec2 pt = _owner->globalToLocal(evt->getPosition());

    int sensitivity;
    //if (Stage.touchScreen)
    sensitivity = UIConfig::touchScrollSensitivity;
    //else
    //sensitivity = 8;

    float diff;
    bool sv = false, sh = false;

    if (_scrollType == ScrollType::ST_VERTICAL)
    {
        if (!_isHoldAreaDone)
        {
            //��ʾ���ڼ�ⴹֱ���������
            _gestureFlag |= 1;

            diff = abs(_beginTouchPos.y - pt.y);
            if (diff < sensitivity)
                return;

            if ((_gestureFlag & 2) != 0) //�Ѿ���ˮƽ����������ڼ�⣬��ô�������ϸ�ķ�ʽ����ǲ��ǰ���ֱ�����ƶ��������ͻ
            {
                float diff2 = abs(_beginTouchPos.x - pt.x);
                if (diff < diff2) //��ͨ�������������
                    return;
            }
        }

        sv = true;
    }
    else if (_scrollType == ScrollType::ST_HORIZONTAL)
    {
        if (!_isHoldAreaDone)
        {
            _gestureFlag |= 2;

            diff = abs(_beginTouchPos.x - pt.x);
            if (diff < sensitivity)
                return;

            if ((_gestureFlag & 1) != 0)
            {
                float diff2 = abs(_beginTouchPos.y - pt.y);
                if (diff < diff2)
                    return;
            }
        }

        sh = true;
    }
    else
    {
        _gestureFlag = 3;

        if (!_isHoldAreaDone)
        {
            diff = abs(_beginTouchPos.y - pt.y);
            if (diff < sensitivity)
            {
                diff = abs(_beginTouchPos.x - pt.x);
                if (diff < sensitivity)
                    return;
            }
        }

        sv = sh = true;
    }

    Vec2 newPos = _containerPos + pt - _beginTouchPos;
    newPos.x = (int)newPos.x;
    newPos.y = (int)newPos.y;

    if (sv)
    {
        if (newPos.y > 0)
        {
            if (!_bouncebackEffect || _inertiaDisabled)
                _container->setPositionY(0);
            else if (_header != nullptr && _header->maxHeight != 0)
                _container->setPositionY((int)MIN(newPos.y * 0.5f, _header->maxHeight));
            else
                _container->setPositionY((int)MIN(newPos.y * 0.5f, _viewSize.height * PULL_RATIO));
        }
        else if (newPos.y < -_overlapSize.height)
        {
            if (!_bouncebackEffect || _inertiaDisabled)
                _container->setPositionY(-_overlapSize.height);
            else if (_footer != nullptr && _footer->maxHeight > 0)
                _container->setPositionY((int)MAX((newPos.y + _overlapSize.height) * 0.5f, -_footer->maxHeight) - _overlapSize.height);
            else
                _container->setPositionY((int)MAX((newPos.y + _overlapSize.height) * 0.5f, -_viewSize.height * PULL_RATIO) - _overlapSize.height);
        }
        else
            _container->setPositionY(newPos.y);
    }

    if (sh)
    {
        if (newPos.x > 0)
        {
            if (!_bouncebackEffect || _inertiaDisabled)
                _container->setPositionX(0);
            else if (_header != nullptr && _header->maxWidth != 0)
                _container->setPositionX((int)MIN(newPos.x * 0.5f, _header->maxWidth));
            else
                _container->setPositionX((int)MIN(newPos.x * 0.5f, _viewSize.width * PULL_RATIO));
        }
        else if (newPos.x < 0 - _overlapSize.width)
        {
            if (!_bouncebackEffect || _inertiaDisabled)
                _container->setPositionX(-_overlapSize.width);
            else if (_footer != nullptr && _footer->maxWidth > 0)
                _container->setPositionX((int)MAX((newPos.x + _overlapSize.width) * 0.5f, -_footer->maxWidth) - _overlapSize.width);
            else
                _container->setPositionX((int)MAX((newPos.x + _overlapSize.width) * 0.5f, -_viewSize.width * PULL_RATIO) - _overlapSize.width);
        }
        else
            _container->setPositionX(newPos.x);
    }

    //�����ٶ�
    auto deltaTime = Director::getInstance()->getDeltaTime();
    float elapsed = (clock() - _lastMoveTime) * 60 - 1;
    if (elapsed > 1) //�ٶ�˥��
        _velocity = _velocity * pow(0.833f, elapsed);
    Vec2 deltaPosition = pt - _lastTouchPos;
    if (!sh)
        deltaPosition.x = 0;
    if (!sv)
        deltaPosition.y = 0;
    _velocity = _velocity.lerp(deltaPosition / deltaTime, deltaTime * 10);

    /*�ٶȼ���ʹ�õ��Ǳ���λ�ƣ����ں����Ĺ��Թ����ж�����Ҫ�õ���Ļλ�ƣ���������Ҫ��¼һ��λ�Ƶı�����
    *�����Ĵ���Ҫʹ�������������ʹ������ת���ķ�����ԭ���ǣ�������UI������UI�У����޷��򵥵ؽ�����Ļ����ͱ��������ת����
    */
    Vec2 deltaGlobalPosition = _lastTouchGlobalPos - evt->getPosition();
    if (deltaPosition.x != 0)
        _velocityScale = abs(deltaGlobalPosition.x / deltaPosition.x);
    else if (deltaPosition.y != 0)
        _velocityScale = abs(deltaGlobalPosition.y / deltaPosition.y);

    _lastTouchPos = pt;
    _lastTouchGlobalPos = evt->getPosition();
    _lastMoveTime = clock();

    //ͬ������posֵ
    if (_overlapSize.width > 0)
        _xPos = clampf(-_container->getPositionX(), 0, _overlapSize.width);
    if (_overlapSize.height > 0)
        _yPos = clampf(-_container->getPositionY(), 0, _overlapSize.height);

    //ѭ�������ر���
    if (_loop != 0)
    {
        newPos = _container->getPosition();
        if (loopCheckingCurrent())
            _containerPos += _container->getPosition() - newPos;
    }

    draggingPane = this;
    _isHoldAreaDone = true;
    _isMouseMoved = true;

    syncScrollBar();
    checkRefreshBar();
    if (_pageMode)
        updatePageController();
    _owner->dispatchEvent(UIEventType::Scroll);
}

void ScrollPane::onTouchEnd(EventContext * context)
{
    InputEvent* evt = context->getInput();

    if (draggingPane == this)
        draggingPane = nullptr;

    _gestureFlag = 0;

    if (!_isMouseMoved || !_touchEffect || _inertiaDisabled)
    {
        _isMouseMoved = false;
        return;
    }

    _isMouseMoved = false;
    _tweenStart = _container->getPosition();

    Vec2 endPos = _tweenStart;
    bool flag = false;
    if (_container->getPositionX() > 0)
    {
        endPos.x = 0;
        flag = true;
    }
    else if (_container->getPositionX() < -_overlapSize.width)
    {
        endPos.x = -_overlapSize.width;
        flag = true;
    }
    if (_container->getPositionY() > 0)
    {
        endPos.y = 0;
        flag = true;
    }
    else if (_container->getPositionY() < -_overlapSize.height)
    {
        endPos.y = -_overlapSize.height;
        flag = true;
    }

    if (flag)
    {
        _tweenChange = endPos - _tweenStart;
        if (_tweenChange.x < -UIConfig::touchDragSensitivity || _tweenChange.y < -UIConfig::touchDragSensitivity)
            _owner->dispatchEvent(UIEventType::PullDownRelease);
        else if (_tweenChange.x > UIConfig::touchDragSensitivity || _tweenChange.y > UIConfig::touchDragSensitivity)
            _owner->dispatchEvent(UIEventType::PullUpRelease);

        if (_headerLockedSize > 0 && getPart(endPos, _refreshBarAxis) == 0)
        {
            setPart(endPos, _refreshBarAxis, _headerLockedSize);
            _tweenChange = endPos - _tweenStart;
        }
        else if (_footerLockedSize > 0 && getPart(endPos, _refreshBarAxis) == -getPart(_overlapSize, _refreshBarAxis))
        {
            float max = getPart(_overlapSize, _refreshBarAxis);
            if (max == 0)
                max = MAX(getPart(_contentSize, _refreshBarAxis) + _footerLockedSize - getPart(_viewSize, _refreshBarAxis), 0);
            else
                max += _footerLockedSize;
            setPart(endPos, _refreshBarAxis, -max);
            _tweenChange = endPos - _tweenStart;
        }

        _tweenDuration.set(TWEEN_TIME_DEFAULT, TWEEN_TIME_DEFAULT);
    }
    else
    {
        //�����ٶ�
        float elapsed = (clock() - _lastMoveTime) * 60 - 1;
        if (elapsed > 1)
            _velocity = _velocity * pow(0.833f, elapsed);

        //�����ٶȼ���Ŀ��λ�ú���Ҫʱ��
        endPos = updateTargetAndDuration(_tweenStart);
        Vec2 oldChange = endPos - _tweenStart;

        //����Ŀ��λ��
        loopCheckingTarget(endPos);
        if (_pageMode || _snapToItem)
            alignPosition(endPos, true);

        _tweenChange = endPos - _tweenStart;
        if (_tweenChange.x == 0 && _tweenChange.y == 0)
            return;

        //���Ŀ��λ���ѵ�������֮������Ҫʱ��
        if (_pageMode || _snapToItem)
        {
            fixDuration(0, oldChange.x);
            fixDuration(1, oldChange.y);
        }
    }

    _tweening = 2;
    _tweenTime.setZero();
    CALL_PER_FRAME(ScrollPane, tweenUpdate);
}

void ScrollPane::onMouseWheel(EventContext * context)
{
    if (!_mouseWheelEnabled)
        return;

    InputEvent* evt = context->getInput();
    int delta = evt->getMouseWheelDelta();
    delta = delta > 0 ? 1 : -1;
    if (_overlapSize.width > 0 && _overlapSize.height == 0)
    {
        if (_pageMode)
            setPosX(_xPos + _pageSize.width * delta, false);
        else
            setPosX(_xPos + _mouseWheelStep * delta, false);
    }
    else
    {
        if (_pageMode)
            setPosY(_yPos + _pageSize.height * delta, false);
        else
            setPosY(_yPos + _mouseWheelStep * delta, false);
    }
}

void ScrollPane::onRollOver(EventContext * context)
{
    showScrollBar(true);
}

void ScrollPane::onRollOut(EventContext * context)
{
    showScrollBar(false);
}

NS_FGUI_END