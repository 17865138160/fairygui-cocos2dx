#ifndef __FUILABEL_H__
#define __FUILABEL_H__

#include "cocos2d.h"
#include "FairyGUIMacros.h"

NS_FGUI_BEGIN

class FUILabel : public cocos2d::Label
{
public:
    FUILabel();
    ~FUILabel();

    CREATE_FUNC(FUILabel);

    const std::string& getFontName() const { return _fontName; }
    void setFontName(const std::string & value);

    int getFontSize() const { return _fontSize; }
    void setFontSize(int value);

    virtual void setTextColor(const cocos2d::Color4B &color) override;

    virtual bool setBMFontFilePath(const std::string& bmfontFilePath, const cocos2d::Vec2& imageOffset = cocos2d::Vec2::ZERO, float fontSize = 0) override;

protected:
    /*
    ע�⣡���������������˱��������Ҫ�޸�cocos2d��Դ�룬�ļ�2d/CCLabel.h����Լ��672�У�ΪupdateBMFontScale��������virtual���η���
    ��Ϊ�����������ǿ���������ָ��ΪFontFnt���͵Ĵ��룬�����ǲ�ʹ��FontFnt��FontFntֻ֧�ִ��ⲿ�ļ����������ã���������BMFontConfiguration�Ƕ�����cpp��ġ���
    ������Ҫ��д���������
    */
    virtual void updateBMFontScale() override;

private:
    int _fontSize;
    std::string _fontName;
};

NS_FGUI_END

#endif
