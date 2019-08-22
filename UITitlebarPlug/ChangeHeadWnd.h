//
// Created by QITMAC000260 on 2019-03-02.
//

#ifndef QTALK_V2_CHANGEHEADWND_H
#define QTALK_V2_CHANGEHEADWND_H

#include "../CustomUi/UShadowWnd.h"
#include "../CustomUi/HeadPhotoLab.h"

/**
* @description: ChangeHeadWnd
* @author: cc
* @create: 2019-03-02 19:38
**/
class ChangeHeadWnd : public UShadowDialog{
    Q_OBJECT
public:
    explicit ChangeHeadWnd(QWidget* parent = nullptr);
    ~ChangeHeadWnd() override;

public:
    void onChangeHeadWnd(const QString& headPath);
    void onShowHead(const QString& headPath);

Q_SIGNALS:
    void sgChangeHead(const QString& headPath);

private:
    void initUi();
    void showChangeHeadWnd();

protected:
    HeadPhotoLab* _pHeadLab;
    QFrame* _bottomFrm;
    QString _headPath;
    QLabel* _pTitleLab;
};


#endif //QTALK_V2_CHANGEHEADWND_H
