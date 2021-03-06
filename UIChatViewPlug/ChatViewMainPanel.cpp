﻿#include <utility>
#include "ChatViewMainPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QDateTime>
#include <QDebug>
#include <sstream>
#include <QApplication>
#include <QPixmap>
#include <QClipboard>
#include <iostream>
#include <QSplitter>
#include <QBuffer>
#include <QMimeData>
#include <QDesktopWidget>
#include <QMediaPlayer>
#include <QTemporaryFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtConcurrent>
#include "StatusWgt.h"
#include "ChatMainWgt.h"
#include "GroupChatSidebar.h"
#include "ToolWgt.h"
#include "InputWgt.h"
#include "../Platform/Platform.h"
#include "../QtUtil/Entity/JID.h"
#include "GroupMember.h"
#include "GroupTopic.h"
#include "../Emoticon/EmoticonMainWgt.h"
#include "../QtUtil/lib/cjson/cJSON_inc.h"
#include "MessagePrompt.h"
#include "../UICom/uicom.h"
#include "../quazip/JlCompress.h"
#include "../Platform/dbPlatForm.h"
#include "../CustomUi/LiteMessageBox.h"
#include "../UICom/qimage/qimage.h"
#include "../Platform/AppSetting.h"
#include "code/SendCodeWnd.h"
#include "code/CodeShowWnd.h"
#include "../CustomUi/QtMessageBox.h"
#include "ShareMessageFrm.h"
#include "../Platform/NavigationManager.h"
#include "SelectUserWnd.h"
#include "MessageItems/VoiceMessageItem.h"
#include "search/LocalSearchMainWgt.h"
#include <math.h>
#include "MessageItems/CodeItem.h"
#include "MessageItems/FileSendReceiveMessItem.h"
#include "QRcode/ScanQRcode.h"

#ifdef _MACOS
#include "sound/PlayAudioSound.h"
#include "MacNotification.h"
#include "../Screenshot/mac/SnipScreenTool.h"
#else
#include "../Screenshot/SnipScreenTool.h"
#endif

#define SYSTEM_XMPPID "SystemMessage"
#define RBT_SYSTEM "rbt-system"
#define RBT_NOTICE "rbt-notice"
#define SYSTEM_CONVERSIONID "SystemConversionId"
#define compare_doule_Equal(x, y) (abs(x - y) < 0.00001)
//
using namespace QTalk;
using namespace QTalk::Entity;

extern ChatViewMainPanel *g_pMainPanel = nullptr;
ChatViewItem::ChatViewItem(const QUInt8 &chatType, const QString &userName, const QTalk::Entity::UID &uid,
                           ChatViewMainPanel *panel)
        : QFrame(),
          _chatType((Enum::ChatType) chatType),
          _leftLay(nullptr),
          _sendBtnLay(nullptr),
          _sendBtn(nullptr) {

    _uid = uid;
    //
    initUi();
    //
    if(_pSearchMainWgt)
        emit _pSearchMainWgt->sgUpdateName(userName);
    _pStatusWgt->switchUser(chatType, uid, userName);
    _pChatMainWgt->clear();
    //
    qRegisterMetaType<QTalk::Entity::ImMessageInfo>("QTalk::Entity::ImMessageInfo");
    qRegisterMetaType<std::string>("std::string");
    connect(this, &ChatViewItem::showMessage, this, &ChatViewItem::showMessageSlot, Qt::QueuedConnection);
    connect(EmoticonMainWgt::getInstance(), &EmoticonMainWgt::sendEmoticon, this, &ChatViewItem::sendEmoticonMessage);
    connect(EmoticonMainWgt::getInstance(), &EmoticonMainWgt::sendCollectionImage, this,
            &ChatViewItem::sendCollectionMessage);

    connect(this, &ChatViewItem::sgRemoveGroupMember, _pInputWgt, &InputWgt::removeGroupMember);
    if(_pGroupSidebar)
        connect(this, &ChatViewItem::sgRemoveGroupMember, _pGroupSidebar->_pGroupMember, &GroupMember::deleteMember);
    connect(this, &ChatViewItem::sgUpdateUserStatus, _pStatusWgt, &StatusWgt::updateUserSts);
    connect(_pShareMessageFrm, &ShareMessageFrm::sgSetShareMessageState, this, &ChatViewItem::setShareMessageState);


    connect(_pToolWgt, &ToolWgt::showSearchWnd, this, &ChatViewItem::onShowSearchWnd);

    qRegisterMetaType<std::vector<QTalk::Entity::ImTransfer>>("std::vector<QTalk::Entity::ImTransfer>");
    connect(this,&ChatViewItem::sgShowSeats,_pToolWgt,&ToolWgt::showSeats);
    connect(this, &ChatViewItem::sgDeleteLater, this, &ChatViewItem::deleteLater, Qt::QueuedConnection);
}

ChatViewItem::~ChatViewItem() {
    if (nullptr != _pStatusWgt) {
        delete _pStatusWgt;
        _pStatusWgt = nullptr;
    }
    if (nullptr != _pChatMainWgt) {
        delete _pChatMainWgt;
        _pChatMainWgt = nullptr;
    }
    if (nullptr != _pGroupSidebar) {
        delete _pGroupSidebar;
        _pGroupSidebar = nullptr;
    }
    if (nullptr != _pInputWgt) {
        delete _pInputWgt;
        _pInputWgt = nullptr;
    }
    if (nullptr != _pToolWgt) {
        delete _pToolWgt;
        _pToolWgt = nullptr;
    }
    if(nullptr != _pSearchMainWgt)
    {
        delete _pSearchMainWgt;
        _pSearchMainWgt = nullptr;
    }
}

void ChatViewItem::setShareMessageState(bool flag)
{
    //
    _pChatMainWgt->setShareMessageState(flag);
    _pInputFrm->setVisible(!flag);
    _pShareMessageFrm->setVisible(flag);
    if(flag)
    {
        _pShareMessageFrm->setFixedHeight(_pInputFrm->height());
        _pShareMessageFrm->setSelectCount(0);
    }
}

/**
  * @函数名   initUi
  * @功能描述 初始化ui
  * @参数
  * @author   cc
  * @date     2018/09/25
  */
void ChatViewItem::initUi() {
    setObjectName("ChatViewItem");
    // first
    _pShareMessageFrm = new ShareMessageFrm(this);
    // top status widget
    _pStatusWgt = new StatusWgt();
    _pStatusWgt->setObjectName("StatusWgt");
    //
    _pChatMainWgt = new ChatMainWgt(this);
    //
    _pGroupSidebar = new GroupChatSidebar(this, _uid.qUsrId());
    //
    _pInputWgt = new InputWgt(_pChatMainWgt, this);
    //
    splitter = new QSplitter(Qt::Vertical, this);
    splitter->setHandleWidth(1);
    //
    _pToolWgt = new ToolWgt(_pInputWgt, this);
    _pToolWgt->setObjectName("ToolWgt");

    _leftLay = new QVBoxLayout;
    _leftLay->setMargin(0);
    _leftLay->setMargin(0);
    //
    _pInputFrm = new QFrame(this);
    auto *inpultLay = new QVBoxLayout(_pInputFrm);
    inpultLay->setContentsMargins(0, 0, 0, 0);
    inpultLay->setSpacing(0);
    inpultLay->addWidget(_pToolWgt);
    inpultLay->addWidget(_pInputWgt);
    //
    splitter->addWidget(_pChatMainWgt);
    splitter->addWidget(_pInputFrm);
    splitter->addWidget(_pShareMessageFrm);
    _leftLay->addWidget(splitter);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    //
    _pShareMessageFrm->setVisible(false);
    //
    _pSearchMainWgt = new LocalSearchMainWgt();
    // 布局
    auto *pMidLayout = new QHBoxLayout;
    pMidLayout->setMargin(0);
    pMidLayout->addLayout(_leftLay);
    if (_chatType == Enum::GroupChat) {
        pMidLayout->addWidget(_pGroupSidebar);
    }
    auto *layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(_pStatusWgt);
    layout->addLayout(pMidLayout);
    setLayout(layout);
    //
    QFrame* sendBtnFrm = new QFrame(this);
    sendBtnFrm->setObjectName("sendBtnFrm");
    _sendBtn = new QPushButton(tr("发送"), this);
    _sendBtn->setObjectName("SendMessageBtn");
    _sendBtn->setFixedSize(100, 30);
    _sendBtnLay = new QHBoxLayout(sendBtnFrm);
    _sendBtnLay->setContentsMargins(0, 5, 15, 5);
    _sendBtnLay->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding));
    _sendBtnLay->addWidget(_sendBtn);

    connect(_sendBtn, &QPushButton::clicked, _pInputWgt, &InputWgt::sendMessage);

    if(AppSetting::instance().getShowSendMessageBtnFlag())
        _leftLay->addWidget(sendBtnFrm);
    else
        sendBtnFrm->setVisible(false);

    std::string username = QTalk::Entity::JID(_uid.usrId().c_str()).username();
    if (_chatType == QTalk::Enum::System || SYSTEM_XMPPID == username || RBT_SYSTEM == username || RBT_NOTICE == username) {
        _pToolWgt->setVisible(false);
        _pInputWgt->setVisible(false);
        _sendBtn->setVisible(false);
    }
//
    connect(_pSearchMainWgt, &LocalSearchMainWgt::sgGetBeforeAllMessage,
        this, &ChatViewItem::onGetBeforeAllMessage);

    connect(_pSearchMainWgt, &LocalSearchMainWgt::sgGetBeforeFileMessage,
            this, &ChatViewItem::onGetBeforeFileMessage);
    connect(_pSearchMainWgt, &LocalSearchMainWgt::sgSearch,
            this, &ChatViewItem::onSearch);

    connect(_pSearchMainWgt, &LocalSearchMainWgt::sgPositionMessage,
            this, &ChatViewItem::onPositionMessage);
}

/**
  * @函数名   conversionId()
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/10/17
  */
QString ChatViewItem::conversionId() {
    if (_strConversionId.isEmpty()) {
        _strConversionId = QString::fromStdString(QTalk::utils::getMessageId());
    }
    return _strConversionId;
}

/**
 *
 * @param id
 * @param messageContent
 */
void ChatViewItem::sendCollectionMessage(const QString &id, const QString& imagePath, const QString& imgLink) {
    if (id != conversionId()) return;

    if(_pInputWgt)
        _pInputWgt->dealFile(imagePath, false, imgLink);

//    if (_pMainPanel && _pMainPanel->getMessageManager()) {
//        long long send_time =
//                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime();
//        std::string msgId = getMessageId();
//        QTalk::Entity::ImMessageInfo message;
//        message.ChatType = _chatType;
//        message.MsgId = msgId;
//        message.To = _uid.usrId();
//        message.RealJid = _uid.realId();
//        message.From = _pMainPanel->getSelfUserId();
//        message.LastUpdateTime = send_time;
//        message.Type = QTalk::Entity::MessageTypePhoto;
//        message.Content = messageContent.toStdString();
//        message.Direction = QTalk::Entity::MessageDirectionSent;
//        message.UserName = Platform::instance().getSelfName();
//
//        S_Message e;
//        e.message = message;
//        e.userId = _uid.usrId();
//        e.time = send_time;
//        e.chatType = _chatType;
//
//        emit _pMainPanel->sgUserSendMessage();
//        _pMainPanel->getMessageManager()->sendMessage(e);
//
//        // 自己显示
//        showMessageSlot(message, false);
//
//    }
}


/**
 *
 */
void ChatViewItem::sendEmoticonMessage(const QString &id, const QString &messageContent, bool isShowAll
        , const std::string& messageId) {
    if (id != conversionId()) return;

    if (isShowAll) {
        _pInputWgt->dealFile(messageContent, false);
        return;
    }

    if (g_pMainPanel && g_pMainPanel->getMessageManager()) {
        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;

        std::string msgId = messageId.empty() ? utils::getMessageId() : messageId;

        QTalk::Entity::ImMessageInfo message;
        message.ChatType = _chatType;
        message.MsgId = msgId;
        message.To = _uid.usrId();
        message.RealJid = _uid.realId();
        message.From = g_pMainPanel->getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = QTalk::Entity::MessageTypeImageNew;
        message.Content = messageContent.toStdString();
        message.Direction = QTalk::Entity::MessageDirectionSent;
        message.UserName = Platform::instance().getSelfName();

        S_Message e;
        e.message = message;
        e.userId = _uid.usrId();
        e.time = send_time;
        e.chatType = _chatType;

        emit g_pMainPanel->sgUserSendMessage();
        g_pMainPanel->getMessageManager()->sendMessage(e);
        if(messageId.empty())
            showMessageSlot(message, false);
    }
}

/**
  * @函数名   showMessageSlot
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/09/20
  */
void ChatViewItem::showMessageSlot(const QTalk::Entity::ImMessageInfo &msg, bool isHis) {

    if (_pChatMainWgt != nullptr) {
        _pChatMainWgt->showMessage(msg, isHis);
    }

    if(_pStatusWgt != nullptr){
        if(msg.ChatType != QTalk::Enum::ChatType::GroupChat && !msg.SendJid.empty()){
            Entity::JID jid(msg.SendJid.c_str());
            _pStatusWgt->showResource(jid.resources());
        }
    }
}

/**
  * @函数名   showMessageSlot
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/09/20
  */
UID ChatViewItem::getPeerId() {
    return _uid;
}

void ChatViewItem::onRecvAddGroupMember(const std::string &memberId, const std::string &nick, int affiliation) {
    if(_pGroupSidebar)
        _pGroupSidebar->updateGroupMember(memberId, nick, affiliation);
}

void ChatViewItem::onRecvRemoveGroupMember(const std::string &memberId) {
    if(_pGroupSidebar)
        _pGroupSidebar->deleteMember(memberId);
}



void ChatViewItem::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Up || e->key() == Qt::Key_Down)
    {
        emit g_pMainPanel->sgShortCutSwitchSession(e->key());
    }

    QWidget::keyPressEvent(e);
}

void ChatViewItem::onShowSearchWnd()
{
    if(nullptr == _pSearchMainWgt)
        return;

    QPoint pos = this->geometry().topRight();
    pos = mapToGlobal(pos);
    _pSearchMainWgt->resetUi();
    _pSearchMainWgt->move({pos.x() - 360, pos.y()});
    _pSearchMainWgt->setVisible(true);
    QApplication::setActiveWindow(_pSearchMainWgt);
    _pSearchMainWgt->raise();

    QPointer<ChatViewItem> pThis(this);
    std::thread([pThis](){

        if(!pThis || !pThis->_pSearchMainWgt)
            return;

        std::vector<QTalk::Entity::ImMessageInfo> allMessage =
                g_pMainPanel->getMessageManager()->getUserLocalHistoryMessage(0, pThis->_uid);

        for(const auto& info : allMessage)
        {
            if(pThis && pThis->_pSearchMainWgt)
                pThis->_pSearchMainWgt->addMessageInfo(info);
        }

        std::vector<QTalk::Entity::ImMessageInfo> fileMessage =
                g_pMainPanel->getMessageManager()->getUserFileHistoryMessage(0, pThis->_uid);

        for(const auto& info : fileMessage)
        {
            if(pThis && pThis->_pSearchMainWgt)
                pThis->_pSearchMainWgt->addFileMessageInfo(info);
        }

    }).detach();
}

void ChatViewItem::onGetBeforeAllMessage(qint64 time)
{
    QPointer<ChatViewItem> pThis(this);
    std::thread([time, pThis](){

        if(!pThis) return;

        std::vector<QTalk::Entity::ImMessageInfo> allMessage =
                g_pMainPanel->getMessageManager()->getUserLocalHistoryMessage(time, pThis->_uid);

        if(allMessage.empty())
        {
            if(pThis && pThis->_pSearchMainWgt)
                pThis->_pSearchMainWgt->setHasBefore(LocalSearchMainWgt::EM_ALL, false);
        }
        else
        {
            for(const auto& info : allMessage)
            {
                if(pThis && pThis->_pSearchMainWgt)
                    pThis->_pSearchMainWgt->addMessageInfo(info);
            }
        }
    }).detach();

}

void ChatViewItem::onGetBeforeFileMessage(qint64 time)
{
    QPointer<ChatViewItem> pThis(this);
    std::thread([time, pThis](){
        if(!pThis) return;

        std::vector<QTalk::Entity::ImMessageInfo> fileMessage =
                g_pMainPanel->getMessageManager()->getUserFileHistoryMessage(time, pThis->_uid);

        if(fileMessage.empty())
        {
            if(pThis && pThis->_pSearchMainWgt)
                pThis->_pSearchMainWgt->setHasBefore(LocalSearchMainWgt::EM_FILE, false);
        }
        else {
            for(const auto& info : fileMessage)
            {
                if(pThis && pThis->_pSearchMainWgt)
                    pThis->_pSearchMainWgt->addFileMessageInfo(info);
            }
        }

    }).detach();

}

/**
 *
 * @param time
 */
void ChatViewItem::onSearch(qint64 time, const QString &text)
{
    QPointer<ChatViewItem> pThis(this);

    std::thread([time, text, pThis](){

        if(!pThis) return;

        std::vector<QTalk::Entity::ImMessageInfo> searchMessage =
                g_pMainPanel->getMessageManager()->getSearchMessage(time, pThis->_uid, text.toLower().toStdString());

        if(searchMessage.empty())
        {
            if(pThis && pThis->_pSearchMainWgt)
                pThis->_pSearchMainWgt->setHasBefore(LocalSearchMainWgt::EM_SEARCH, false);
        }
        else {
            for(const auto& info : searchMessage)
            {
                if(pThis && pThis->_pSearchMainWgt)
                    pThis->_pSearchMainWgt->addSearchMessageInfo(info, text);
            }
        }

    }).detach();
}

void ChatViewItem::onPositionMessage(qint64 time)
{
    QPointer<ChatViewItem> pThis(this);
    std::thread([time, pThis](){

        if(!pThis) return;

        std::vector<QTalk::Entity::ImMessageInfo> afterMessage =
                g_pMainPanel->getMessageManager()->getAfterMessage(time, pThis->_uid);
        for(const auto& info : afterMessage)
        {
            if(pThis && pThis->_pSearchMainWgt)
                pThis->_pSearchMainWgt->addMessageInfo(info);
        }

    }).detach();
}

//*******************ChatViewMainPanel********************//


ChatViewMainPanel::ChatViewMainPanel(QWidget *parent) :
        QFrame(parent),
        _strSelfId(""),
        _pool("ChatViewMainPanel pool"),
        _pEmptyLabel(nullptr) {
    g_pMainPanel = this;
    setObjectName("ChatViewMainPanel");
    _pCodeShowWnd = new CodeShowWnd(this);
    _pSendCodeWnd = new SendCodeWnd(this);
    _pVideoPlayWnd = new VideoPlayWnd(this);
    _pSelectUserWnd = new SelectUserWnd(this);

    _pQRcode = new QRcode(this);
    //
    _pStackedLayout = new QStackedLayout(this);
    showEmptyLabel();
    //
    _pMessageManager = new ChatMsgManager;
    _pMessageListener = new ChatMsgListener;
    gifManager = new GIFManager;
    //
	QString fileConfigPath = QString("%1/fileMsgPath").arg(Platform::instance().getConfigPath().data());
    _pFileMsgConfig = new QTalk::ConfigLoader(fileConfigPath.toLocal8Bit());
    _pFileMsgConfig->reload();
    // 初始化提示音
    initSound();
    // 加载截屏 表情插件
    _pSnipScreenTool = SnipScreenTool::getInstance();
    EmoticonMainWgt::getInstance();
    initPlug();
    //
    _pAudioVideoManager = new AudioVideoManager;
    connect(_pAudioVideoManager, &AudioVideoManager::sgSendSignal, this, &ChatViewMainPanel::onSendSignal);
    connect(_pAudioVideoManager, &AudioVideoManager::sgClose2AudioVideo,
            [this](const QString& peerId, bool isVideo, long long occupied_time){
        QString json = QString("{\"type\":\"close\", \"time\": %1, \"desc\":\"\" }").arg(occupied_time);
        sendAudioVideoMessage(QTalk::Entity::UID(peerId), QTalk::Enum::TwoPersonChat, isVideo, json);
    });
    //
    qRegisterMetaType<QTalk::Entity::ImMessageInfo>("QTalk::Entity::ImMessageInfo");
    qRegisterMetaType<std::string>("std::string");
    connect(this, &ChatViewMainPanel::showMessagePromptSignal, this, &ChatViewMainPanel::showPrompt);
    connect(_pSnipScreenTool, &SnipScreenTool::sgFinish, this, &ChatViewMainPanel::onSnapFinish);
    connect(_pSnipScreenTool, &SnipScreenTool::sgCancel, this, &ChatViewMainPanel::onSnapCancel);
    connect(this, &ChatViewMainPanel::sgReloadChatView, this, &ChatViewMainPanel::reloadChatView);
    connect(this, &ChatViewMainPanel::sgDisconnected, this, &ChatViewMainPanel::onDisConnected);
    connect(this, &ChatViewMainPanel::sgShowLiteMessageBox, this, &ChatViewMainPanel::showLiteMessageBox);
    connect(this, &ChatViewMainPanel::sgShowInfoMessageBox, this, &ChatViewMainPanel::showInfoMessageBox);
    connect(this, &ChatViewMainPanel::sgPlaySound, this, &ChatViewMainPanel::playSound);
    connect(this, &ChatViewMainPanel::sgShowVidioWnd, this, &ChatViewMainPanel::onShowVideoWnd);
    connect(this, &ChatViewMainPanel::showQRcode, [this](){
        if(_pQRcode->isVisible())
            _pQRcode->setVisible(false);
        _pQRcode->showCenter(false, this);
    });
    //
    QPointer<ChatViewMainPanel> pThis(this);
    std::thread([pThis](){
        if(pThis && pThis->_pMessageManager)
        {
            std::vector<QTalk::StShareSession> ss;
            pThis->_pMessageManager->getContactsSession(ss);

            if(!pThis) return;

            if(pThis && !ss.empty())
                pThis->_pSelectUserWnd->setSessions(SelectUserWnd::EM_TYPE_CONTACTS, ss);
        }
    }).detach();

    std::function<int(STLazyQueue<ChatViewItem*>*)> deleteItemFuc = [this](STLazyQueue<ChatViewItem*> *q) ->int {
        int runningCount = 0;
        if (q != nullptr && !q->empty()) {
            while (!q->empty()) {
                auto *item = q->front();
                if(item)
                    emit item->sgDeleteLater();
                //
                q->pop();
                runningCount++;
            }
        }
        return runningCount;
    };
    _deleteItemQueue = new STLazyQueue<ChatViewItem*>(500, deleteItemFuc);
}

ChatViewMainPanel::~ChatViewMainPanel() {

    for (MessagePrompt *msgPrompt : _arMessagePrompt) {
        delete msgPrompt;
        msgPrompt = nullptr;
    }
}

/**
  * @函数名   onChatUserChanged
  * @功能描述 接受切换聊天会话请求
  * @参数
  * @author   cc
  * @date     2018/09/19
  */
void ChatViewMainPanel::onChatUserChanged(const StSessionInfo &info) {

    if (nullptr != _pStackedLayout) {
        UID uid = UID(info.userId, info.realJid);

        ChatViewItem * item = nullptr;
        {
//            QMutexLocker locker(&_mutex);
            item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
            if (item ) {
                if(item->_uid == uid)
                {
                    item->_pInputWgt->setFocus();
                    return;
                }
//                item->_pChatMainWgt->clearData();
//                if(AppSetting::instance().getAutoDeleteSession())
//                {
//                    _pStackedLayout->removeWidget(item);
//                    _mapSession.remove(item->_uid);
//                }
            }
        }

        // 先进入聊天窗口再拉历史消息
        auto func = [this, info, uid]() {
            // 切换主画面
            if (_mapSession.contains(uid)) {
                ChatViewItem *item = _mapSession.value(uid);
                if (nullptr != item) {
                    if (_pStackedLayout->indexOf(item) == -1)
                        _pStackedLayout->addWidget(item);

                    _pStackedLayout->setCurrentWidget(item);
                    if(_mapSession[uid] && _mapSession[uid]->_pInputWgt->isVisible())
                        _mapSession[uid]->_pInputWgt->setFocus();
                    else if(_mapSession[uid])
                        _mapSession[uid]->setFocus();

                    QString conversionId = item->conversionId();
                    _pSnipScreenTool->setConversionId(conversionId);

                    if(item && item->_pSearchMainWgt)
                        emit item->_pSearchMainWgt->sgInitStyle(_qss);
                }
                //
                if(item && item->_pChatMainWgt)
                    item->_pChatMainWgt->setConnectState(_isConnected);

                if(_his_input_data.contains(uid))
                {
                    if(item && item->_pInputWgt)
                        item->_pInputWgt->setContentByJson(_his_input_data[uid]);
                    _his_input_data.remove(uid);
                }
            }
        };
        int chatType = info.chatType;
        //
        if(dbPlatForm::instance().isHotLine(uid.usrId()))
        {
            if(uid.usrId() == uid.realId()){
                chatType = QTalk::Enum::Consult;
                std::thread([this, uid]() {
                    if(_pMessageManager)
                        _pMessageManager->hotLineMessageList(uid.usrId());
                }).detach();
            } else {
                chatType = QTalk::Enum::ConsultServer;
            }
        }
        //
        if (!_mapSession.contains(uid) || _mapSession[uid] == nullptr) {
            addItem(chatType, info.userName, uid);
            //
            func();

            //
            if (_pMessageManager) {
                std::thread([this, info, uid, item, chatType]() {
#ifdef _MACOS
                    pthread_setname_np("ChatViewMainPanel::onChatUserChanged");
#endif
                    // 获取历史消息
                    QInt64 t = 0;

                    info_log("onChatUserChanged time:{0}", t);
                    std::vector<QTalk::Entity::ImMessageInfo> msgLst = _pMessageManager->getUserHistoryMessage(t,
                                                                                                               info.chatType,
                                                                                                               uid);
                    auto it = msgLst.begin();
                    for (; it != msgLst.end(); it++) {
                        QTalk::Entity::ImMessageInfo msg = *it;
                        if (it->ChatType == Enum::GroupChat) {
                            if (it->HeadSrc.empty())
                            {
#ifdef _STARTALK
                                msg.HeadSrc = ":/QTalk/image1/StarTalk_defaultHead.png";
#else
                                msg.HeadSrc = ":/QTalk/image1/headPortrait.png";
#endif
                            }
                            else
                                msg.HeadSrc = _pMessageManager->getLocalHeadPath(it->HeadSrc);
                        } else {
                            msg.HeadSrc = info.headPhoto.toStdString();
                        }

                        {
                            QMutexLocker locker(&_mutex);
                            _mapHeadPath[QString::fromStdString(it->From)] = QString::fromStdString(msg.HeadSrc);
                        }

                        switch (it->Type) {
                            case -1: {
                                cJSON *extend = cJSON_Parse(it->ExtendedInfo.c_str());
                                if (cJSON_HasObjectItem(extend, "fromId")) {
                                    char *fromId = cJSON_GetObjectItem(extend, "fromId")->valuestring;
                                    msg.XmppId = fromId;
                                }
                                cJSON_Delete(extend);
                                break;
                            }
                            default:
                                break;
                        }

                        if(msg.ChatType == QTalk::Enum::GroupChat)
                        {
                            auto uinfo = dbPlatForm::instance().getUserInfo(msg.From);
                            if(nullptr == uinfo)
                                msg.UserName = QTalk::Entity::JID(msg.From.data()).username();
                            else
                                msg.UserName = QTalk::getUserName(uinfo);
                        }

                        if (SYSTEM_XMPPID == QTalk::Entity::JID(it->XmppId.c_str()).username()) {
                            msg.HeadSrc = ":/chatview/image1/system.png";
                            _mapHeadPath[QString::fromStdString(it->XmppId)] = QString::fromStdString(msg.HeadSrc);
                        }
                        if(_mapSession[uid])
                            emit _mapSession[uid]->showMessage(msg, false);
                    }
                    // 获取群名称
                    if (chatType == Enum::GroupChat && _pMessageManager) {
                        _pMessageManager->getGroupInfo(info.userId.toStdString());
                    }

                }).detach();
            }
        } else {
            //
            func();
            updateHead();
        }


        // delete old item
//        QApplication::processEvents(QEventLoop::AllEvents, 100);
        if(item)
        {
            QString content = item->_pInputWgt->translateText();
            if(!content.isEmpty())
            {
//                if(AppSetting::instance().getAutoDeleteSession())
//                    _his_input_data[item->_uid] = content;
                emit sgShowDraft(item->_uid, content);
            }
//            if(AppSetting::instance().getAutoDeleteSession())
//            {
//
//            }
        }
    }
}

/**
  * @函数名   onRecvMessage
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/09/20
  */
void ChatViewMainPanel::onRecvMessage(R_Message &e) {

    QTalk::Entity::ImMessageInfo message = e.message;
    Entity::JID jid(message.SendJid.c_str());
    Entity::JID from(message.From.c_str());
    std::string userId = jid.barename();
    std::string realJid = message.RealJid.empty() ? userId : message.RealJid;
    UID uid(userId, realJid);

    {
        QMutexLocker locker(&_mutex);
        if (from.barename() != getSelfUserId())
        {
            message.Direction = QTalk::Entity::MessageDirectionReceive;
            static QInt64 recvMessageTime = 0;
            QInt64 currentTime = QDateTime::currentMSecsSinceEpoch();
            if (currentTime - recvMessageTime >= 1000 &&
                _mapNotice.find(userId) == _mapNotice.end()) {

                if(/*message.ChatType == QTalk::Enum::GroupChat &&*/ message.UserName.empty())
                {
                    auto info = dbPlatForm::instance().getUserInfo(message.RealFrom);
                    if(nullptr == info && _pMessageManager)
                    {
                       info = std::make_shared<QTalk::Entity::ImUserInfo>();
                       info->XmppId = message.RealFrom;
                        _pMessageManager->getUserInfo(info);
                    }
                    message.UserName = QTalk::getUserName(info);
                }

                recvMessageTime = currentTime;
                // 消息提示
                emit recvMessageSignal();
                // 消息提示音
#ifndef _MACOS
                if (AppSetting::instance().getNewMsgAudioNotify())
                        emit sgPlaySound();
#endif

                // 消息提示窗
                if (AppSetting::instance().getNewMsgTipWindowNotify())
                        emit showMessagePromptSignal(message);
            }
        }
        else
        {
            message.Direction = QTalk::Entity::MessageDirectionSent;
            emit sgUserSendMessage();
        }

        if(message.Type == QTalk::Entity::MessageTypeShock)
        {
            onShowChatWnd(message.ChatType, QString::fromStdString(userId),
                    QString::fromStdString(realJid), QString::fromStdString(message.UserName), getSelfUserId().data());
            emit sgShockWnd();
        }

//        if(message.Type == QTalk::Entity::WebRTC_MsgType_Video && message.ChatType == QTalk::Enum::TwoPersonChat)
//        {
//            onRecvVideo(message.From);
//        }

        if(!message.AutoReply && message.ChatType == QTalk::Enum::TwoPersonChat && from.barename() != getSelfUserId())
            sendAutoPeplyMessage(userId, message.MsgId);

        // 没有显示的会话不处理
        if (!_mapSession.contains(uid) || nullptr == _mapSession[uid] /*|| _mapSession[uid] != _pStackedLayout->currentWidget()*/) {
            info_log("not show message from {0} messageId:{1}", uid.toStdString(), message.MsgId);
            return;
        }

        //
        QTalk::Entity::JID jidSender(message.From.c_str());
        QString sender = QString::fromStdString(jidSender.barename());

        if (_mapHeadPath.contains(sender)) {
            message.HeadSrc = _mapHeadPath[sender].toStdString();
        }

        if (SYSTEM_XMPPID == QTalk::Entity::JID(message.XmppId.data()).username()) {
            message.HeadSrc = ":/chatview/image1/system.png";
            _mapHeadPath[QString::fromStdString(message.XmppId)] = QString::fromStdString(message.HeadSrc);
        }

        if(message.ChatType == QTalk::Enum::GroupChat)
        {
            auto info = dbPlatForm::instance().getUserInfo(message.RealFrom);
            if(info)
                message.UserName = QTalk::getUserName(info);
        }
    }

//    info_log("emit signal showMessage from:{0} messageId:{1}", uid.toStdString(), message.MsgId);
    if(_mapSession[uid])
        emit _mapSession[uid]->showMessage(message, false);
}

void ChatViewMainPanel::onActiveMainWindow() {
    QWidget *mainWgt = UICom::getInstance()->getAcltiveMainWnd();
    if (mainWgt) {
        mainWgt->activateWindow();

        if (mainWgt->isMinimized()) {
            mainWgt->setWindowState((mainWgt->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        } else {
            mainWgt->setVisible(true);
            QApplication::setActiveWindow(mainWgt);
        }
    }
}

/**
  * @函数名
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/10/19
  */
void updateItemProcess(ChatViewItem *item, const FileProcessMessage &e)
{
    if (item && item->_pChatMainWgt && item->_pSearchMainWgt) {
        item->_pChatMainWgt->recvFileProcess(e.speed, e.downloadTotal, e.downloadNow, e.uploadTotal,
                                             e.uploadNow, e.key);
        item->_pSearchMainWgt->recvFileProcess(e.speed, e.downloadTotal, e.downloadNow, e.uploadTotal,
                                               e.uploadNow, e.key);
    }
}

void ChatViewMainPanel::onRecvFileProcessMessage(const FileProcessMessage &e) {

//    QMutexLocker locker(&_mutex);
    if((e.downloadTotal > 0 && compare_doule_Equal(e.downloadTotal, e.downloadNow)) ||
            (e.uploadTotal > 0 && compare_doule_Equal(e.uploadTotal, e.uploadNow)))
    {
        for(const auto& item : _mapSession)
            if(item) updateItemProcess(item, e);
    }
    else
    {
        auto item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
        updateItemProcess(item, e);
    }
}

/**
  * @函数名   getSelfUserId
  * @功能描述 获取自身ID
  * @参数
  * @author   cc
  * @date     2018/09/20
  */
std::string ChatViewMainPanel::getSelfUserId() {
    if (_strSelfId.empty()) {
        _strSelfId = Platform::instance().getSelfXmppId();
    }
    return _strSelfId;
}

/**
  * @函数名   updateGroupMember
  * @功能描述 更新群列表
  * @参数
  * @author   cc
  * @date     2018/10/08
  */
void ChatViewMainPanel::updateGroupMember(const GroupMemberMessage &e) {
    //
    UID uid(e.groupId);
    auto itFind = _mapSession.find(uid);
    if (itFind != _mapSession.end() && *itFind && (*itFind)->_chatType == Enum::GroupChat) {
        if (_mapSession[uid] && _mapSession[uid]->_pGroupSidebar)
            _mapSession[uid]->_pGroupSidebar->updateGroupMember(e);
        if (_mapSession[uid] && _mapSession[uid]->_pInputWgt)
            _mapSession[uid]->_pInputWgt->updateGroupMember(e.members);
    }
    // todo
//    updateUserHeadPath(e.members);
}

/**
  * @函数名   updateHead
  * @功能描述 更新头像
  * @参数
  * @author   cc
  * @date     2018/10/09
  */
void ChatViewMainPanel::updateHead() {

//    std::thread t([this]() {
//#ifdef _MACOS
//        pthread_setname_np("ChatViewMainPanel::updateHead");
//#endif
//        QMutexLocker locker(&_mutex);
//        if(nullptr == _pStackedLayout)
//            return;
//
//        auto item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
////        for (ChatViewItem *item: _mapSession) {
//            if (item && item->_pGroupSidebar && item->_pGroupSidebar->_pGroupMember) {
//                item->_pGroupSidebar->_pGroupMember->updateHead();
//            }
////        }
//    });
//    t.detach();

    auto func = [this](){
        if(nullptr == _pStackedLayout)
            return;
        if(_pStackedLayout->count())
        {
            auto item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
            if (item && item->_pGroupSidebar && item->_pGroupSidebar->_pGroupMember) {
                item->_pGroupSidebar->_pGroupMember->updateHead();
            }
        }
    };

//    if(Platform::instance().isMainThread())
    QtConcurrent::run(func);
//    else func();
}

/**
  * @函数名
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/10/09
  */
void ChatViewMainPanel::updateUserHeadPath(const std::vector<QTalk::StUserCard> &users) {
    std::thread t([this, users]() {
#ifdef _MACOS
        pthread_setname_np("ChatViewMainPanel::updateUserHeadPath");
#endif
        QMutexLocker locker(&_mutex);
        for (const auto &user : users) {
            std::string headSrc = QTalk::GetHeadPathByUrl(user.headerSrc);
            _mapHeadPath[QString(user.xmppId.c_str())] = QString(headSrc.c_str());
        }
    });
    t.detach();
}

/**
  * @函数名   updateGroupTopic
  * @功能描述 更新群公告
  * @参数
  * @author   cc
  * @date     2018/10/12
  */
void ChatViewMainPanel::updateGroupTopic(const std::string &groupId, const std::string &topic) {
    UID uid(groupId);
    if (_mapSession.contains(uid) && _mapSession[uid] && _mapSession[uid]->_pGroupSidebar) {
        emit _mapSession[uid]->_pGroupSidebar->updateGroupTopic(QString(topic.c_str()));
    }
}

/**
  * @函数名   getOfflineMessage
  * @功能描述 获取断链期间消息
  * @参数
  * @author   cc
  * @date     2018/10/25
  */
void ChatViewMainPanel::getOfflineMessage() {


}

/**
  * @函数名
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/10/26
  */
void
ChatViewMainPanel::updateGroupMemberInfo(const std::string &groupId, const std::vector<QTalk::StUserCard> &userCards) {
    std::thread t([this, groupId, userCards]() {
#ifdef _MACOS
        pthread_setname_np("ChatViewMainPanel::updateGroupMemberInfo");
#endif
        QMutexLocker locker(&_mutex);
        UID uid(groupId);
        if (_mapSession.contains(uid)) {
            if(_mapSession[uid] && _mapSession[uid]->_pGroupSidebar && _mapSession[uid]->_pGroupSidebar->_pGroupMember)
                _mapSession[uid]->_pGroupSidebar->_pGroupMember->updateMemberInfo(userCards);
            if(_mapSession[uid] && _mapSession[uid]->_pInputWgt)
                _mapSession[uid]->_pInputWgt->updateGroupMemberInfo(userCards);
        }
        updateUserHeadPath(userCards);
    });
    t.detach();
}

/**
  * @函数名
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/10/26
  */
ChatMsgManager *ChatViewMainPanel::getMessageManager() {
    if (nullptr != _pMessageManager) {
        return _pMessageManager;
    }
    return nullptr;
}

/**
  * @函数名   setConnectStatus
  * @功能描述 与服务器连接状态
  * @参数
  * @author   cc
  * @date     2018/10/25
  */
void ChatViewMainPanel::setConnectStatus(bool isConnect) {
    QMutexLocker locker(&_mutex);
    _isConnected = isConnect;
    if (_isConnected) {
        emit sgReloadChatView();
    }
    else
    {
        emit sgDisconnected();
    }
}

/**
  * @函数名   getConnectStatus
  * @功能描述 获取与服务器连接状态
  * @参数
     bool 连接状态
  * @author   cc
  * @date     2018/10/25
  */
bool ChatViewMainPanel::getConnectStatus() {
    QMutexLocker locker(&_mutex);
    return _isConnected;
}

/**
  * @函数名   addItem
  * @功能描述 创建子Item
  * @参数
  * @author   cc
  * @date     2018/09/25
  */
void ChatViewMainPanel::addItem(const QUInt8 &chatType, const QString &userName, const QTalk::Entity::UID &uid) {
    //if (!_mapSession.contains(userId) || _mapSession[userId] == nullptr)
    {
        auto *item = new ChatViewItem(chatType, userName, uid, this);
        _pStackedLayout->addWidget(item);
        _mapSession[uid] = item;
    }
}

/**
  * @函数名
  * @功能描述 加载截屏 表情插件
  * @参数
  * @author   cc
  * @date     2018/10/30
  */
void ChatViewMainPanel::initPlug() {
    //
    QString styleSheetPath = QString(":/style/style1/Screenshot.qss");
    if (QFile::exists(styleSheetPath)) {
        QFile file(styleSheetPath);
        if (file.open(QFile::ReadOnly)) {
            QString qss = QString::fromUtf8(file.readAll()); //以utf-8形式读取文件
            _pSnipScreenTool->setToolbarStyle(qss.toUtf8());
            file.close();
        }
    }
}

//
void ChatViewMainPanel::showUserCard(const QString &userId) {
    emit showUserCardSignal(userId);
}

/**
 * 
 */
void ChatViewMainPanel::getHistoryMsg(const QInt64 &t, const QUInt8 &chatType, const QTalk::Entity::UID &uid) {

    if(Platform::instance().isMainThread()) {
        _pool.enqueue(std::bind(&ChatViewMainPanel::getHistoryMsg, this, t, chatType, uid));
//        QtConcurrent::run(this, &ChatViewMainPanel::getHistoryMsg, t, chatType, uid);
    }

    std::vector<QTalk::Entity::ImMessageInfo> msgLst = _pMessageManager->getUserHistoryMessage(t, chatType, uid);
    //
    if (msgLst.empty() && _mapSession[uid] && _mapSession[uid]->_pChatMainWgt) {
        _mapSession[uid]->_pChatMainWgt->setHasNotHistoryMsgFlag(false);
    }

    for (auto &it : msgLst) {

        if (SYSTEM_XMPPID == QTalk::Entity::JID(it.XmppId.c_str()).username()) {
            it.HeadSrc = ":/chatview/image1/system.png";
            _mapHeadPath[uid.qUsrId()] = QString::fromStdString(it.HeadSrc);
        }

        if (it.ChatType == Enum::GroupChat) {
            if (it.HeadSrc.empty())
            {
#ifdef _STARTALK
                it.HeadSrc = ":/QTalk/image1/StarTalk_defaultGroup.png";
#else
                it.HeadSrc = ":/QTalk/image1/defaultGroupHead.png";
#endif
            }
            else
                it.HeadSrc = _pMessageManager->getLocalHeadPath(it.HeadSrc);
        } else
            it.HeadSrc = _mapHeadPath[uid.qUsrId()].toStdString();

        _mapHeadPath[QString::fromStdString(it.From)] = QString::fromStdString(it.HeadSrc);

        switch (it.Type) {
            case -1: {
                cJSON *extend = cJSON_Parse(it.ExtendedInfo.c_str());
                //                msg.messageType = EM_SHOWTYPE_REVOKE;
                if (cJSON_HasObjectItem(extend, "fromId")) {
                    char *fromId = cJSON_GetObjectItem(extend, "fromId")->valuestring;
                    it.XmppId = fromId;
                }
                cJSON_Delete(extend);
                break;
            }
            default:
                break;
        }

        if(it.ChatType == QTalk::Enum::GroupChat /*&& it.UserName.empty()*/)
        {
            auto info = dbPlatForm::instance().getUserInfo(it.From);
            if(nullptr == info && _pMessageManager)
            {
                info = std::make_shared<QTalk::Entity::ImUserInfo>();
                info->XmppId = it.From;
                _pMessageManager->getUserInfo(info);
            }
            it.UserName = QTalk::getUserName(info);
        }
        QPointer<ChatViewItem> pItem(_mapSession[uid]);
        if(pItem)
            emit pItem->showMessage(it, true);
        else
            return;
    }
}

//
void ChatViewMainPanel::gotReadState(const UID &uid, const map<string, QInt32> &msgMasks) {

    if (_mapSession.contains(uid) && nullptr != _mapSession[uid]) {
        emit _mapSession[uid]->_pChatMainWgt->gotReadStatueSignal(msgMasks);
    }
}

void ChatViewMainPanel::gotMState(const QTalk::Entity::UID &uid, const std::string &messageId, const QInt64 &time) {
    if (_mapSession.contains(uid) && nullptr != _mapSession[uid]) {
        emit _mapSession[uid]->_pChatMainWgt->sgGotMState(messageId.data(), time);
    }
}

void ChatViewMainPanel::updateRevokeMessage(const QTalk::Entity::UID &uid,
                                            const std::string &fromId, const std::string &messageId, const long long time) {
    if (_mapSession.contains(uid) && nullptr != _mapSession[uid] && _mapSession[uid]->_pChatMainWgt) {
        emit _mapSession[uid]->_pChatMainWgt->updateRevokeSignal(QString::fromStdString(fromId),
                                                                 QString::fromStdString(messageId), time);
    }
}

/**
  * @函数名
  * @功能描述
  * @参数
  * @author   cc
  * @date     2018/12/13
  */
void ChatViewMainPanel::onDestroyGroup(const std::string &groupId, bool isDestroy) {
    QTalk::Entity::UID uid(groupId);
    if (_mapSession.contains(uid) && _mapSession[uid]) {
        ChatViewItem *wgt = _mapSession[uid];
        if (_pStackedLayout->currentWidget() == wgt) {
            _pStackedLayout->setCurrentIndex(0);
        }
        _pStackedLayout->removeWidget(wgt);
        _mapSession.remove(uid);
    }
    //
    QString name = QTalk::getGroupName(groupId).data();
    emit sgShowInfoMessageBox(isDestroy ? QString(tr("群 %1 已被销毁")).arg(name) :
                              QString(tr("您已退出/被移出群聊 %1")).arg(name));
}

QString dealMessageContent(const QTalk::Entity::ImMessageInfo &msg) {

    QString ret = "";
    if (msg.ChatType == QTalk::Enum::GroupChat && !msg.UserName.empty()) {
        ret += QString(msg.UserName.data()) + ": ";
    }

    switch (msg.Type)
    {

        case QTalk::Entity::MessageTypeFile:
            ret += QObject::tr("[文件]");
            break;
        case QTalk::Entity::MessageTypePhoto:
            ret += QObject::tr("[图片]");
            break;
        case QTalk::Entity::MessageTypeImageNew:
            ret += QObject::tr("[表情]");
            break;
        case QTalk::Entity::MessageTypeSmallVideo:
            ret += QObject::tr("[视频]");
            break;
        case QTalk::Entity::WebRTC_MsgType_VideoCall:
            ret += QObject::tr("[实时视频]");
            break;
        case QTalk::Entity::WebRTC_MsgType_AudioCall:
            ret += QObject::tr("[实时音频]");
            break;
        case QTalk::Entity::WebRTC_MsgType_Video_Group:
            ret += QObject::tr("[群组视频]");
            break;
        case QTalk::Entity::MessageTypeCommonTrdInfo:
        case QTalk::Entity::MessageTypeCommonTrdInfoV2:
            ret += QObject::tr("[链接卡片]");
            break;
        case QTalk::Entity::MessageTypeSourceCode:
            ret += QObject::tr("[代码块]");
            break;
        case QTalk::Entity::MessageTypeVoice:
            ret += QObject::tr("[语音]");
            break;
        case 65537:
        case 65538:
            ret += QObject::tr("[热线提示信息]");
            break;
        default:
        case QTalk::Entity::MessageTypeText:
        case QTalk::Entity::MessageTypeGroupAt:
        case QTalk::Entity::MessageTypeRobotAnswer:
        {
            QString tmpContent = QString::fromStdString(msg.Content).split("\n").first();

            QRegExp regExp("\\[obj type=[\\\\]?\"([^\"]*)[\\\\]?\" value=[\\\\]?\"([^\"]*)[\\\\]?\"(.*)\\]");
            regExp.setMinimal(true);

            int pos = 0;
            while ((pos = regExp.indexIn(tmpContent)) != -1) {
                QString item = regExp.cap(0); // 符合条件的整个字符串
                QString type = regExp.cap(1); // 多媒体类型

                if ("url" == type) {
                    tmpContent.replace(pos, item.size(), QObject::tr("[链接]"));
                } else if ("image" == type) {
                    tmpContent.replace(pos, item.size(), QObject::tr("[图片]"));
                } else if ("emoticon" == type) {
                    tmpContent = QObject::tr("[表情]");
                } else {
                    tmpContent.replace(pos, item.size(), QObject::tr("[未知类型]"));
                }
            }

            ret += tmpContent;
            break;
        }
    }

    return ret;
}

void getParam(const QTalk::Entity::ImMessageInfo &msg, StNotificationParam& param)
{
    //
    QTalk::Entity::JID jid(msg.SendJid.data());
    std::string headPath;
    if(msg.ChatType == QTalk::Enum::GroupChat)
    {
        std::shared_ptr<QTalk::Entity::ImGroupInfo> groupInfo = dbPlatForm::instance().getGroupInfo(jid.barename(), true);
        if(nullptr != groupInfo)
        {
            param.title = groupInfo->Name;
            headPath = groupInfo->HeaderSrc;
        }

        if(!headPath.empty())
        {
            headPath = QTalk::GetHeadPathByUrl(headPath);
        }
        if(headPath.empty() || !QFile::exists(headPath.data()))
        {
#ifdef _STARTALK
            headPath = ":/QTalk/image1/StarTalk_defaultGroup.png";
#else
            headPath = ":/QTalk/image1/defaultGroupHead.png";
#endif
        }
    }
    else if(msg.ChatType == QTalk::Enum::System)
    {
        headPath = ":/chatview/image1/system.png";
        param.title = QObject::tr("系统消息").toStdString();
    }
    else
    {
        std::shared_ptr<QTalk::Entity::ImUserInfo> userInfo = dbPlatForm::instance().getUserInfo(jid.barename());
        if(nullptr != userInfo)
        {
            param.title = QTalk::getUserName(userInfo);
            headPath = userInfo->HeaderSrc;
        }

        if(!headPath.empty())
        {
            headPath = QTalk::GetHeadPathByUrl(headPath);
        }
        if(headPath.empty() || !QFile::exists(headPath.data()))
        {
#ifdef _STARTALK
            headPath = ":/QTalk/image1/StarTalk_defaultHead.png";
#else
            headPath = ":/QTalk/image1/headPortrait.png";
#endif
        }
    }

    param.icon = headPath;
    param.message = dealMessageContent(msg).toStdString();
    param.playSound = AppSetting::instance().getNewMsgAudioNotify();
    param.xmppId = Entity::JID(msg.SendJid.c_str()).barename();
    param.from = Entity::JID(msg.From.c_str()).barename();
    param.realJid = Entity::JID(msg.RealJid.c_str()).barename();
    param.chatType = msg.ChatType;
}

// 显示提示消息
void ChatViewMainPanel::showPrompt(const QTalk::Entity::ImMessageInfo &msg) {

    StNotificationParam param;
    getParam(msg, param);

#if defined(_MACOS)
    param.loginUser = getSelfUserId();
    QTalk::mac::Notification::showNotification(&param);
#else
    if( AppSetting::instance().getUseNativeMessagePrompt()) {
        emit sgShowNotify(param);
        return;
    }
    //
    auto *prompt = new MessagePrompt(msg);
    prompt->setAttribute(Qt::WA_QuitOnClose, false);
    connect(prompt, &MessagePrompt::openChatWnd, this, &ChatViewMainPanel::onShowChatWnd);
    // 计算位置
    QDesktopWidget *deskTop = QApplication::desktop();
    int curMonitor = deskTop->screenNumber(this);
    QRect deskRect = deskTop->screenGeometry(curMonitor);
    //
    auto showFun = [this, prompt, deskRect]() {
        prompt->adjustSize();
        prompt->setVisible(true);
        prompt->startToshow(QPoint(deskRect.right(), deskRect.top() + 80 + _arMessagePrompt.size() * 80));
        _arMessagePrompt.push_back(prompt);
    };
    // 设置最多显示 超过之后干掉 其他移动到上面
    if (_arMessagePrompt.size() >= 3) {

        for (MessagePrompt *tmpPrompt : _arMessagePrompt) {
            tmpPrompt->moveTop();
        };

        auto *tmpPrompt = _arMessagePrompt[0];
        //prompt->setParent(nullptr);
        delete tmpPrompt;
        tmpPrompt = nullptr;
        _arMessagePrompt.remove(0);
    }
    showFun();
#endif
}

void ChatViewMainPanel::removePrompt(MessagePrompt *prompt) {

    int pos = _arMessagePrompt.lastIndexOf(prompt);
    if (pos != -1) {
        {
            QMutexLocker locker(&_mutex);
            _arMessagePrompt.remove(pos);
        }
        prompt->setParent(nullptr);
        delete prompt;
        prompt = nullptr;
    }
}

void ChatViewMainPanel::setNoticeMap(const std::map<std::string, std::string> &mapNotice) {

    QMutexLocker locker(&_mutex);
    _mapNotice = mapNotice;
}

void ChatViewMainPanel::updateUserConfig(const std::vector<QTalk::Entity::ImConfig> &configs) {

    std::map<std::string, std::string> tmpNotice;
    auto it = configs.begin();
    for (; it != configs.end(); it++) {
        if (it->ConfigKey == "kNoticeStickJidDic") {
            tmpNotice[it->ConfigSubKey] = it->ConfigValue;
        }
    }
    //
    updateUserMaskName();
    //
    QMutexLocker locker(&_mutex);
    _mapNotice = tmpNotice;
}

void ChatViewMainPanel::updateUserConfig(const std::map<std::string, std::string> &deleteData,
                                         const std::vector<QTalk::Entity::ImConfig> &arImConfig) {
    QMutexLocker locker(&_mutex);
    for(const auto& it : deleteData)
    {
        if(it.second == "kMarkupNames")
        {
            UID uid(it.first);
            if(_mapSession.contains(uid) && _mapSession[uid])
            {
                auto info = dbPlatForm::instance().getUserInfo(uid.usrId(), true);
                std::string userName = QTalk::getUserName(info);
                emit _mapSession[uid]->_pStatusWgt->updateName(userName.data());
                emit _mapSession[uid]->_pSearchMainWgt->sgUpdateName(userName.data());
            }
        }
        else if(it.second == "kNoticeStickJidDic")
        {
            _mapNotice.erase(it.first);
        }
    }
    //
    for(const auto& conf : arImConfig)
    {

        if(conf.ConfigKey == "kMarkupNames")
        {
            UID uid(conf.ConfigSubKey);
            if(_mapSession.contains(uid) && _mapSession[uid])
            {
                auto info = dbPlatForm::instance().getUserInfo(uid.usrId(), true);
                std::string userName = QTalk::getUserName(info);
                emit _mapSession[uid]->_pStatusWgt->updateName(userName.data());
                emit _mapSession[uid]->_pSearchMainWgt->sgUpdateName(userName.data());
            }
        }
        else if(conf.ConfigKey == "kNoticeStickJidDic")
        {
            _mapNotice[conf.ConfigSubKey] = conf.ConfigValue;
        }
    }
}

/**
 * 更新用户maskname
 */
void ChatViewMainPanel::updateUserMaskName()
{
    auto it = _mapSession.begin();
    for(; it != _mapSession.end(); it++)
    {
        if(*it && (*it)->_chatType == QTalk::Enum::TwoPersonChat)
        {
            auto info = dbPlatForm::instance().getUserInfo(it.key().usrId(), true);
            std::string userName = QTalk::getUserName(info);
            if((*it)->_pStatusWgt)
                emit (*it)->_pStatusWgt->updateName(userName.data());
            if((*it)->_pSearchMainWgt)
                emit (*it)->_pSearchMainWgt->sgUpdateName(userName.data());
        }
    }
}

void ChatViewMainPanel::setFileMsgLocalPath(const std::string &key, const std::string &val) {
    _pFileMsgConfig->setString(key, val);
    _pFileMsgConfig->saveConfig();
}

std::string ChatViewMainPanel::getFileMsgLocalPath(const std::string &key) {
    std::string ret = _pFileMsgConfig->getString(key);
    return _pFileMsgConfig->getString(key);
}

void ChatViewMainPanel::onAppDeactivated() {
    auto item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
    if (item && item->_pInputWgt) {
        item->_pInputWgt->onAppDeactivated();
        item->_pSearchMainWgt->setVisible(false);
    }
}

void ChatViewMainPanel::onShowChatWnd(QUInt8 chatType, QString userId, QString realJid,
        QString strUserName, QString loginUser) {
    //
    info_log("onShowChatWnd user:{0} loginUser:{1}", userId.toStdString(), loginUser.toStdString());

    if(loginUser.toStdString() != getSelfUserId())
        return;

    StSessionInfo stSession(chatType, std::move(userId), std::move(strUserName));
    stSession.realJid = std::move(realJid);
    emit sgSwitchCurFun(0);
    emit sgOpenNewSession(stSession);
    //
    emit sgWakeUpWindow();
}

/**
 *
 * @param fromId
 * @param messageId
 */
void ChatViewMainPanel::recvBlackMessage(const std::string &fromId, const std::string &messageId) {

    UID uid(fromId);
    if (_mapSession.contains(uid) && _mapSession[uid] && _mapSession[uid]->_pChatMainWgt) {
        _mapSession[uid]->_pChatMainWgt->recvBlackListMessage(QString::fromStdString(messageId));
    }
}

/**
 *
 * @param key
 * @param path
 */
void ChatViewMainPanel::addConllection(const QString &subkey, const QString &path) {
    if (_pMessageManager) {
        _pMessageManager->setUserSetting(false, "kCollectionCacheKey", subkey.toStdString(), path.toStdString());
    }
}

void ChatViewMainPanel::showEmptyLabel() {
    if (nullptr == _pEmptyLabel) {
        _pEmptyLabel = new QLabel(this);
        _pEmptyLabel->setObjectName("ChatViewEmptyLabel");
        _pEmptyLabel->setAlignment(Qt::AlignCenter);
        _pStackedLayout->addWidget(_pEmptyLabel);
    }

    QTime curTime = QTime::currentTime();
    int hour = curTime.hour();
    QPixmap emptyPixmap;
    if (hour < 12)
        emptyPixmap = QTalk::qimage::instance().loadPixmap(QString(":/chatview/image1/morning_%1.png").arg(AppSetting::instance().getThemeMode()), false);
    else if (hour < 19)
        emptyPixmap = QTalk::qimage::instance().loadPixmap(QString(":/chatview/image1/afternoon_%1.png").arg(AppSetting::instance().getThemeMode()), false);
    else
        emptyPixmap = QTalk::qimage::instance().loadPixmap(QString(":/chatview/image1/night_%1.png").arg(AppSetting::instance().getThemeMode()), false);

    _pEmptyLabel->setPixmap(emptyPixmap);

    _pStackedLayout->setCurrentWidget(_pEmptyLabel);
}

/**
 * 更新群资料信息
 * @param groupinfo
 */
void ChatViewMainPanel::updateGroupInfo(const std::shared_ptr<QTalk::StGroupInfo>& groupinfo) {
    UID uid(groupinfo->groupId);
    if (_mapSession.contains(uid) && nullptr != _mapSession[uid]) {
        emit _mapSession[uid]->_pGroupSidebar->updateGroupTopic(QString::fromStdString(groupinfo->title));
        emit _mapSession[uid]->_pStatusWgt->updateName(QString::fromStdString(groupinfo->name));
        emit _mapSession[uid]->_pSearchMainWgt->sgUpdateName(QString::fromStdString(groupinfo->name));
    }
}

void deleteDictionary(const QString& dirPath)
{
    QFileInfo info(dirPath);
    if(info.isDir())
    {
        QDir baseDir(dirPath);
        auto infoList = baseDir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for(const auto& tmpInfo : infoList)
        {
            if(tmpInfo.isDir())
            {
                deleteDictionary(tmpInfo.absoluteFilePath());
                baseDir.rmpath(tmpInfo.absoluteFilePath());
            }
            else {
                QFile::remove(tmpInfo.absoluteFilePath());
            }
        }
    }
    else
    {
        QFile::remove(dirPath);
    }
}

void deleteHistoryLogs()
{
    QString logBasePath = QString("%1/logs").arg(Platform::instance().getAppdataRoamingPath().data());

    QDir baseDir(logBasePath);
    auto infoList = baseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for(const auto& info : infoList)
    {
        QString dirName = info.baseName();
        auto dirDate = QDate::fromString(dirName, "yyyy-MM-dd");
        auto now = QDate::currentDate();
        if(dirDate.addDays(5) <= now)
        {
            deleteDictionary(info.absoluteFilePath());
            baseDir.rmpath(info.absoluteFilePath());
        }
    }
}

void deleteDumps()
{
    QString logBasePath = QString("%1/logs").arg(Platform::instance().getAppdataRoamingPath().data());
    //
#ifdef _WINDOWS
    std::function<void(const QString&)> delDmpfun;
            delDmpfun = [&delDmpfun](const QString& path) {

                QDir dir(path);
                QFileInfoList infoList = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
                for (QFileInfo tmpInfo : infoList)
                {
                    if (tmpInfo.isDir())
                    {
                        delDmpfun(tmpInfo.absoluteFilePath());
                    }
                    else if (tmpInfo.isFile())
                    {
                        if (tmpInfo.suffix().toLower() == "dmp")
                        {
                            QFile::remove(tmpInfo.absoluteFilePath());
                        }
                    }
                }
            };

    delDmpfun(logBasePath);
#endif
}

/**
 * 打包发送日志 dump
 */
void ChatViewMainPanel::packAndSendLog(const QString &describe) {

    QtConcurrent::run([this, describe]() {
#ifdef _MACOS
        pthread_setname_np("ChatViewMainPanel::packAndSendLog");
#endif
        //
        int isNotify = ("@@#@@" != describe.toStdString());

        if (isNotify)
            emit sgShowLiteMessageBox(true, tr("开始收集日志..."));
        //db 文件
        QString dbPath = QString::fromStdString(Platform::instance().getAppdataRoamingUserPath()) + "/qtalk.db";
        QString logBasePath = QString::fromStdString(Platform::instance().getAppdataRoamingPath()) + "/logs";
        QString logDbPath = logBasePath + "/qtalk.db";
        if (QFile::exists(logDbPath))
            QFile::remove(logDbPath);
        if(QFile::exists(dbPath))
            QFile::copy(dbPath, logDbPath);
        dbPath = QString::fromStdString(Platform::instance().getAppdataRoamingUserPath()) + "/qtalk.db-shm";
        logDbPath = logBasePath + "/qtalk.db-shm";
        if (QFile::exists(logDbPath))
            QFile::remove(logDbPath);
        if(QFile::exists(dbPath))
            QFile::copy(dbPath, logDbPath);
        dbPath = QString::fromStdString(Platform::instance().getAppdataRoamingUserPath()) + "/qtalk.db-wal";
        logDbPath = logBasePath + "/qtalk.db-wal";
        if (QFile::exists(logDbPath))
            QFile::remove(logDbPath);
        if(QFile::exists(dbPath))
            QFile::copy(dbPath, logDbPath);

        //
        if (isNotify)
            emit sgShowLiteMessageBox(true, tr("日志收集完毕 开始打包..."));

        QString logZip = logBasePath + "/log.zip";
        auto index = 0;
        bool packOk = false;
        while (index < 2 && !packOk)
        {
            // zip
            if (QFile::exists(logZip))
                QFile::remove(logZip);

            bool ret = JlCompress::compressDir(logZip, logBasePath);
            if(ret)
            {
                QFileInfo logFileInfo(logZip);
                // 检测日志大小
                if(logFileInfo.size() >= 1024 * 1024 * 1024)
                {
//                if (isNotify)
//                    emit sgShowLiteMessageBox(true, "日志文件太大, 增加删除历史日志!");
                    if(index == 0)
                        deleteHistoryLogs();
                    else if(index == 1)
                        deleteDumps();

                    index++;
                }
                else
                    packOk = true;
            }
            else
                break;
        }

        if(packOk)
        {
            if (isNotify)
                emit sgShowLiteMessageBox(true, tr("日志打包成功 开始上传日志并发送邮件..."));
            if (_pMessageManager) {
                _pMessageManager->sendLogReport(describe.toStdString(), logZip.toStdString());
            }
        }
        else
        {
            if (isNotify)
                emit sgShowInfoMessageBox(tr("日志反馈失败，请重试..."));
        }
    });
}

/**
* 截屏
*/
void ChatViewMainPanel::systemShortCut() {
    _pSnipScreenTool->setConversionId(SYSTEM_CONVERSIONID);
    _pSnipScreenTool->Init().Start();
#ifdef _MACOS
    //onSnapFinish(SYSTEM_CONVERSIONID);
#endif
}

void ChatViewMainPanel::onSnapFinish(const QString &id) {
    if (id == SYSTEM_CONVERSIONID) {
        //
        auto *item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
        if (nullptr != item) {
            _pSnipScreenTool->setConversionId(item->conversionId());
        }
        //
        QString localPath = QString("%1/image/temp/").arg(Platform::instance().getAppdataRoamingUserPath().c_str());
        QString fileName = localPath + QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz.png");
        bool bret = QApplication::clipboard()->pixmap().save(fileName, "PNG");
        if (bret) {
            // 将截图放到 剪切板
            auto *mimeData = new QMimeData;
//            mimeData->setUrls(QList<QUrl>() << QUrl::fromLocalFile(fileName));
            mimeData->setImageData(QImage(fileName));
            QApplication::clipboard()->setMimeData(mimeData);

            if (_pStackedLayout->currentIndex() != -1) {
                if(this->isActiveWindow())
                {
                    auto *item = qobject_cast<ChatViewItem *>(_pStackedLayout->currentWidget());
                    if (nullptr != item) {
                        item->_pInputWgt->onPaste();
                    }
                }
            }
        }
    } else {
        for (auto &item : _mapSession.values()) {
            if(item)
                item->_pInputWgt->onSnapFinish(id);
        }
    }
}

void ChatViewMainPanel::onSnapCancel(const QString &id) {
    if (id == SYSTEM_CONVERSIONID) {
    } else {
        for (auto &item : _mapSession.values()) {
            if(item)
                item->_pInputWgt->onSnapCancel(id);
        }
    }
}

/**
 * 清除会话
 * @param id
 */
void ChatViewMainPanel::onRemoveSession(const QTalk::Entity::UID &id) {
    if (_mapSession.contains(id) && _mapSession[id]) {
        QMutexLocker locker(&_mutex);
        ChatViewItem *item = _mapSession[id];
        _mapSession.remove(id);
        _pStackedLayout->removeWidget(item);
        item->deleteLater();
    }
    showEmptyLabel();
}

/**
 *
 */
void ChatViewMainPanel::onRemoveGroupMember(const std::string &groupId, const std::string &memberId) {
    //
    UID uid(groupId);
    if (_mapSession.contains(uid) && _mapSession[uid]) {
        emit _mapSession[uid]->sgRemoveGroupMember(memberId);
    }
}

/**
 *
 */
void ChatViewMainPanel::reloadChatView() {
    QMutexLocker locker(&_mutex);
    //
    _pStackedLayout->setCurrentWidget(_pEmptyLabel);
    for (auto& view : _mapSession.values()) {
        delete view;
        view = nullptr;
    }
    _mapSession.clear();
}

//
void ChatViewMainPanel::showLiteMessageBox(bool isSuccess, const QString &message) {
    if (isSuccess)
        LiteMessageBox::success(message, 2000);
    else
        LiteMessageBox::failed(message, 2000);
}


void ChatViewMainPanel::showInfoMessageBox(const QString &message)
{
    QtMessageBox::information(this, tr("提示"), message);
}

//
void ChatViewMainPanel::onLogReportMessageRet(bool isSuccess, const std::string &message) {
    emit sgShowInfoMessageBox(QString::fromStdString(message));
}

/**
 * 
 */
void ChatViewMainPanel::recvUserStatus(const std::map<std::string, std::string> &userstatus) {

    //
    auto it = userstatus.begin();
    for (; it != userstatus.end(); it++) {
        QString userId = QString(it->first.c_str());
        //
        UID uid(userId, userId);
        if (_mapSession.contains(uid) && _mapSession[uid]) {
            emit _mapSession[uid]->sgUpdateUserStatus(QString::fromStdString(it->second));
        }
    }
}


/**
 * 播放收到消息的提示音
 */
void ChatViewMainPanel::playSound() {
#ifndef _MACOS
    if (_pPlayer->state() != QMediaPlayer::StoppedState)
        _pPlayer->stop();
    _pPlayer->play();
#else

    QString dirctionPath = QString::fromStdString(AppSetting::instance().getgetNewMsgAudioPath());
    const QString &soundPath = ":/chatview/sound/msg.wav";
    if ((dirctionPath.isEmpty() || !QFile::exists(dirctionPath)) && QFile::exists(soundPath)) {
        dirctionPath = QString("%1/msg.wav").arg(Platform::instance().getAppdataRoamingPath().data());
        QTemporaryFile* tmpFile = QTemporaryFile::createNativeFile(soundPath); // Returns a pointer to a temporary file
        tmpFile->copy(dirctionPath);
        AppSetting::instance().setgetNewMsgAudioPath(dirctionPath.toStdString());
    }

    if (QFile::exists(dirctionPath)) {
        QTalk::mac::util::PlayAudioSound::PlaySound(dirctionPath.toStdString().c_str());
    }

#endif
}

void ChatViewMainPanel::initSound() {

#ifndef _MACOS
    _pPlayer = new QMediaPlayer;
    const QString &soundPath = ":/chatview/sound/msg.wav";
	QString dirctionPath = QString::fromStdString(AppSetting::instance().getgetNewMsgAudioPath());
    if ((dirctionPath.isEmpty() || !QFile::exists(dirctionPath)) && QFile::exists(soundPath)) {
		dirctionPath = QString("%1/msg.wav").arg(Platform::instance().getAppdataRoamingPath().data());
		QTemporaryFile* tmpFile = QTemporaryFile::createNativeFile(soundPath); // Returns a pointer to a temporary file
		tmpFile->copy(dirctionPath);
		AppSetting::instance().setgetNewMsgAudioPath(dirctionPath.toStdString());
    }
    if (QFile::exists(dirctionPath)) {
        _pPlayer->setMedia(QUrl::fromLocalFile(dirctionPath));
    }
#endif

    _pVoicePlayer = new QMediaPlayer;
}

void ChatViewMainPanel::playVoice(const std::string &localFile, VoiceMessageItem *msgItem)
{
    if(_pVoicePlayer)
    {
        if(_pVoicePlayer->state() != QMediaPlayer::PlayingState)
            _pVoicePlayer->stop();

        _pVoicePlayer->setMedia(QUrl::fromLocalFile(localFile.data()));
        _pVoicePlayer->play();

        connect(_pVoicePlayer, &QMediaPlayer::stateChanged, [msgItem](QMediaPlayer::State newState){
            if(newState == QMediaPlayer::StoppedState)
            {
                msgItem->stopVoice();
            }
        });
    }
}

///**
// * 2人视频
// */
//void ChatViewMainPanel::onRecvVideo(const string &userId) {
//
//}

void ChatViewMainPanel::onRecvAddGroupMember(const std::string &groupId, const std::string &memberId,
                                             const std::string &nick, int affiliation) {

    UID uid(groupId);
    QMutexLocker locker(&_mutex);
    if(_mapSession.contains(uid))
    {
        ChatViewItem *item = _mapSession.value(uid);
        if (item) {
            item->onRecvAddGroupMember(memberId, nick, affiliation);
        }
    }
}

void ChatViewMainPanel::onRecvRemoveGroupMember(const std::string &groupId, const std::string &memberId) {
    UID uid(groupId);
    ChatViewItem *item = _mapSession.value(uid);
    if (item) {
        item->onRecvRemoveGroupMember(memberId);
    }
}

void ChatViewMainPanel::onShowVideoWnd(const std::string &caller, const std::string &callee, bool isVideo) {
    start2Talk_old(caller, isVideo, false);
}


void ChatViewMainPanel::showShowCodeWnd(const QString &type, const QString &language, const QString &content) {
    if (_pCodeShowWnd) {
        _pCodeShowWnd->show();
        _pCodeShowWnd->hide();

        _pCodeShowWnd->showCode(type, language, content);
        _pCodeShowWnd->showCenter(false, this);
        QApplication::setActiveWindow(_pCodeShowWnd);
    }
}

void ChatViewMainPanel::showSendCodeWnd(const UID &uid) {
    if (_pSendCodeWnd && _mapSession.contains(uid) && _mapSession[uid]) {

        _pSendCodeWnd->show();
        _pSendCodeWnd->hide();

        auto item = _mapSession[uid];

        QString code = item->_pInputWgt->toPlainText();
        _pSendCodeWnd->addCode(uid, code);
        //
        if (_pSendCodeWnd->isVisible())
            _pSendCodeWnd->setVisible(false);


        _pSendCodeWnd->showCenter(false, nullptr);
    }
}

/**
 * send code message
 * @param uid
 * @param text
 * @param codeType
 * @param codeLanguage
 */
void ChatViewMainPanel::sendCodeMessage(const UID &uid, const std::string &text, const std::string &codeType,
                                        const std::string &codeLanguage) {
    if (_mapSession.contains(uid) && _mapSession[uid]) {
        sendCodeMessage(uid, _mapSession[uid]->_chatType, text, codeType, codeLanguage);
    }
}

/**
 *
 * @param localHead
 */
void ChatViewMainPanel::onChangeHeadSuccess(const std::string &localHead) {
    std::string xmppId = getSelfUserId();
    {
        QMutexLocker locker(&_mutex);
        _mapHeadPath[QString::fromStdString(xmppId)] = QString::fromStdString(localHead);
    }
}

/**
 *
 * @param mood
 */
void ChatViewMainPanel::onUpdateMoodSuccess(const std::string& userId, const std::string &mood)
{
    UID uid(userId);
    if(_mapSession.contains(uid) && _mapSession[uid] && _mapSession[uid]->_pStatusWgt)
    {
        emit _mapSession[uid]->_pStatusWgt->updateMood(QString::fromStdString(mood));
    }
    //

}

/**
 *
 */
void ChatViewMainPanel::onAppActive() {

}

/**
 *
 */
void ChatViewMainPanel::onMsgSoundChanged()
{
#ifdef _WINDOW
	_pPlayer->setMedia(QUrl());
#endif

	QString soundPath = QFileDialog::getOpenFileName(this, tr("请选择一个提示音"), Platform::instance().getHistoryDir().data(), "mp3 wav(*.mp3 *.wav)");
	if (soundPath.isEmpty())
		return;
	const QString &dirctionPath = QString::fromStdString(AppSetting::instance().getgetNewMsgAudioPath());
	QFile::remove(dirctionPath);
	QString newSoundPath = QString("%1/%2").arg(Platform::instance().getAppdataRoamingPath().data()).arg(QFileInfo(soundPath).fileName());
	if (QFileInfo(newSoundPath).exists() && !QFile::remove(dirctionPath))
	{
		emit sgShowLiteMessageBox(false, tr("提示音 文件替换失败"));
		return;
	}
	QFile::copy(soundPath, newSoundPath);

	AppSetting::instance().setgetNewMsgAudioPath(newSoundPath.toStdString());
#ifndef _MACOS

	if (QFile::exists(newSoundPath)) {
		_pPlayer->setMedia(QUrl::fromLocalFile(newSoundPath));
	}
#else
	QTalk::mac::util::PlayAudioSound::removeSound(dirctionPath.toStdString().data());
#endif
}

/**
 *
 * @param localFile
 */
std::string ChatViewMainPanel::uploadFile(const std::string& localFile)
{
    std::string ret;
    if(_pMessageManager)
    {
        ret = _pMessageManager->uploadFile(localFile);
    }
    return ret;
}

/**
 *
 * @param url
 */
void ChatViewMainPanel::sendShareMessage(const QString& id, int chatType, const QString& shareUrl)
{
    UID uid(id);
    //
    _pSelectUserWnd->reset();
    _pSelectUserWnd->showCenter(true, this);
    _pSelectUserWnd->_loop->quit();
    _pSelectUserWnd->_loop->exec();
    //
    QString title;
    if(chatType == QTalk::Enum::GroupChat)
    {
        auto info = dbPlatForm::instance().getGroupInfo(uid.usrId());
        if(info)
            title = QString("%1的聊天记录").arg(info->Name.data());
        else
            title = QString(tr("群聊记录"));
    }
    else
    {
        std::string userId = uid.usrId();
        std::string selfName = Platform::instance().getSelfName();
        if(userId == getSelfUserId())
        {
            title = QString("%1的聊天记录").arg(selfName.data());
        }
        else
        {
            auto info = dbPlatForm::instance().getUserInfo(uid.usrId());
            if(info)
                title = QString("%1和%2的聊天记录").arg(info->Name.data()).arg(selfName.data());
            else
                title = QString("%1的聊天记录").arg(selfName.data());
        }
    }

    QString linkurl = QString("%1?jdata=%2").arg(NavigationManager::instance().getShareUrl().data())
            .arg(QUrl(shareUrl.toUtf8().toBase64()).toEncoded().data());

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "title", title.toUtf8());
    cJSON_AddStringToObject(obj, "desc", "");
    cJSON_AddStringToObject(obj, "linkurl", linkurl.toUtf8());
    std::string extenInfo = QTalk::JSON::cJSON_to_string(obj);
    cJSON_Delete(obj);

    // 发送消息
    long long send_time =
            QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;
    QTalk::Entity::ImMessageInfo message;
    message.From = getSelfUserId();
    message.LastUpdateTime = send_time;
    message.Type = QTalk::Entity::MessageTypeCommonTrdInfo;
    message.Content = QObject::tr("收到了一个消息记录文件文件，请升级客户端查看。").toStdString();
    message.ExtendedInfo = extenInfo;
    message.Direction = QTalk::Entity::MessageDirectionSent;
    message.UserName = Platform::instance().getSelfName();

    S_Message e;
    e.time = send_time;

    //
    if(_pSelectUserWnd->getEvtRet() == 1 && !_pSelectUserWnd->_selectedIds.empty())
    {
        auto it = _pSelectUserWnd->_selectedIds.begin();
        for(; it != _pSelectUserWnd->_selectedIds.end(); it++)
        {
            message.ChatType = it->second;
            message.To = it->first.usrId();
            message.RealJid = it->first.realId();
            message.MsgId = QTalk::utils::getMessageId();
            e.message = message;
            e.userId = it->first.usrId();
            e.chatType = it->second;
            if(_mapSession.contains(it->first) && _mapSession[it->first])
            {
                emit _mapSession[it->first]->showMessage(message, false);
            }
            emit sgUserSendMessage();
            _pMessageManager->sendMessage(e);
        }
    }
}

/**
 *
 * @param type
 * @param key
 */
void ChatViewMainPanel::searchLocalSession(int type, const std::string& key)
{
    //
    std::vector<QTalk::StShareSession> sessions;
    if(_pMessageManager)
    {
        switch (type)
        {
            case SelectUserWnd::EM_TYPE_RECENT:
                _pMessageManager->getRecentSession(sessions);
                break;
            case SelectUserWnd::EM_TYPE_CONTACTS:
                break;
            case SelectUserWnd::EM_TYPE_GROUPCHAT:
                break;
            default:
                break;
        }
    }

    if(_pSelectUserWnd)
    {
        _pSelectUserWnd->setSessions(type, sessions);
    }
}

void ChatViewMainPanel::forwardMessage(const std::string &messageId) {
    // 获取需要转发的会话
    _pSelectUserWnd->reset();
    _pSelectUserWnd->showCenter(true, this);
    _pSelectUserWnd->_loop->quit();
    _pSelectUserWnd->_loop->exec();
    //
    std::map<std::string, int> users;
    auto it = _pSelectUserWnd->_selectedIds.begin();
    for(; it != _pSelectUserWnd->_selectedIds.end(); it++)
    {
        users[it->first.usrId()] = it->second;
    }
    //
    if (_pMessageManager) {
        _pMessageManager->forwardMesssage(messageId, users);
    }
}

/**
 *
 * @param filePath
 * @param linkFile
 */
void ChatViewMainPanel::makeFileLink(const QString& filePath, const QString& fileMd5)
{
    QString linkDir = QString("%1/fileLink").arg(Platform::instance().getAppdataRoamingUserPath().data());

    if(!QFile::exists(linkDir))
    {
        QDir dir;
        dir.mkpath(linkDir);
    }

#ifdef _WINDOWS
	QString linkFile = linkDir.append("/").append(fileMd5).append(".lnk");
#else
	QString linkFile = linkDir.append("/").append(fileMd5);
#endif // _WINDOWS
   
    // 保证最新
    QFile::remove(linkFile);
    // 创建软链
    bool mkLink = QFile::link(filePath, linkFile);
	info_log("make file link {0} -> {1} result:{2}", filePath.toStdString(), linkFile.toStdString(), mkLink);
}

QString ChatViewMainPanel::getFileLink(const QString& fileMd5)
{
    if(!fileMd5.isEmpty())
    {
        return QString("%1/fileLink/%2").arg(Platform::instance().getAppdataRoamingUserPath().data()).arg(fileMd5);
    }
    return QString();
}

/**
 *
 */
void ChatViewMainPanel::setAutoReplyFlag(bool flag)
{
    _autoReply = flag;
}

void ChatViewMainPanel::sendAutoPeplyMessage(const std::string& receiver, const std::string& messageId) {

    bool atuoReplyFalg = AppSetting::instance().getLeaveCheckEnable();
    if (_autoReply && atuoReplyFalg && _pMessageManager)
    {

        std::string autoReplyMessage = AppSetting::instance().getAutoReplyMsg();

        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;

        std::string msgId = "auto_reply_" + messageId;

        QTalk::Entity::ImMessageInfo message;
        message.ChatType = QTalk::Enum::TwoPersonChat;
        message.MsgId = msgId;
        message.To = receiver;
        message.RealJid = receiver;
        message.From = getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = QTalk::Entity::MessageTypeText;
        message.Content = QString("[ 自动回复 ]: %1").arg(autoReplyMessage.data()).toStdString();
        message.UserName = Platform::instance().getSelfName();
        message.AutoReply = true;
        message.Direction = QTalk::Entity::MessageDirectionSent;

        S_Message e;
        e.message = message;
        e.userId = receiver;
        e.time = send_time;
        e.chatType = QTalk::Enum::TwoPersonChat;

        _pMessageManager->sendMessage(e);

//        UID uid(receiver);
//        if(_mapSession.contains(uid))
//            emit _mapSession[uid]->showMessage(message, false);
    }
}

void ChatViewMainPanel::setSeatList(const QTalk::Entity::UID& uid,
                                    const std::vector<QTalk::Entity::ImTransfer>& transfers) {
    if(_mapSession.contains(uid) && _mapSession[uid]){
        emit _mapSession[uid]->sgShowSeats(transfers);
    }
}

void ChatViewMainPanel::setSearchStyle(const QString& qss)
{
    _qss = qss;
    //
    QString styleSheetPath = QString(":/style/style%1/Emoticon.qss").arg(AppSetting::instance().getThemeMode());
    if (QFile::exists(styleSheetPath)) {
        QFile file(styleSheetPath);
        if (file.open(QFile::ReadOnly)) {
            QString qss = QString::fromUtf8(file.readAll()); //以utf-8形式读取文件
            EmoticonMainWgt::getInstance()->setStyleSheet(qss.toUtf8());
            file.close();
        }
    }

    for(auto pItem : _mapSession)
    {
        if(pItem)
            pItem->_pSearchMainWgt->initStyle(qss);
    }
    // 强制刷新ui
    resize(this->width() - 10, this->height());
    resize(this->width() + 10, this->height());
}

void ChatViewMainPanel::updateGroupReadMState(const std::string &groupId, const std::vector<std::string>& ids) {
    UID uid(groupId);
    if(_mapSession.contains(uid) && _mapSession[uid] && _mapSession[uid]->_pChatMainWgt)
    {
        emit _mapSession[uid]->_pChatMainWgt->sgGotGroupMStatue(ids);
    }
}

void ChatViewMainPanel::onDisConnected()
{
    auto* curItem = qobject_cast<ChatViewItem*>( _pStackedLayout->currentWidget());
    // 先刷当前item
    if(nullptr != curItem)
    {
        emit curItem->_pChatMainWgt->sgDisConnected();
    }
    // 再刷其他item
    for(const auto& session : _mapSession)
    {
        if(session && session != curItem && session->_pChatMainWgt)
            emit session->_pChatMainWgt->sgDisConnected();
    }
}

/**
 *
 * @param msgInfo
 */
void ChatViewMainPanel::resendMessage(MessageItemBase* item)
{
    QApplication::processEvents(QEventLoop::AllEvents, 100);
    UID uid;
    const QTalk::Entity::ImMessageInfo& message = item->msgInfo();
    if(message.ChatType == QTalk::Enum::TwoPersonChat)
    {
        uid = UID(message.RealJid);
    }
    else if(message.ChatType == QTalk::Enum::GroupChat)
    {
        uid = UID(message.RealJid);
    }
    else if(message.ChatType == QTalk::Enum::Consult)
    {
        uid = UID(message.From, message.RealJid);
    }

    if(_mapSession.contains(uid) && _mapSession[uid])
    {
        switch (message.Type)
        {
            case QTalk::Entity::MessageTypeText:
            case QTalk::Entity::MessageTypeGroupAt:
            {
                std::thread([message, uid, item, this](){
                    std::map<std::string, std::string> mapAt;
                    bool success = false;
                    QString content = message.Content.data();
                    if(content.isEmpty())
                        content = getRealText(message.ExtendedInfo.data(), message.MsgId.data(), success, mapAt);
                    else
                        success = true;
                    if(success)
                        sendTextMessage(uid, message.ChatType, content.toStdString(), mapAt, message.MsgId);
                    else
                    {
                        if(item)
                            item->onDisconnected();
                        error_log("{0} message resend failed reason: convert message item failed", message.MsgId);
                        return;
                    }
                }).detach();
                break;
            }
            case QTalk::Entity::MessageTypeSourceCode:
            {
                auto* codeItem = qobject_cast<CodeItem*>(item);
                if(codeItem)
                {
                    sendCodeMessage(uid, message.ChatType, codeItem->_code.toStdString(),
                                                      codeItem->_codeStyle.toStdString(), codeItem->_codeLanguage.toStdString(), message.MsgId);
                }
                else
                {
                    item->onDisconnected();
                    error_log("{0} message resend failed reason: convert message item failed", message.MsgId);
                    QtMessageBox::warning(this, tr("警告"), tr("消息重发失败!"));
                }
                break;
            }
            case QTalk::Entity::MessageTypeFile:
            {
                cJSON *content = cJSON_Parse(message.ExtendedInfo.data());
                if (nullptr == content) {
                    item->onDisconnected();
                    error_log("{0} message resend failed reason: file not exists", message.MsgId);
                    QtMessageBox::warning(this, tr("警告"), tr("原文件不存在, 消息重发失败!"));
                    break;
                }
                //
                std::string filePath = QTalk::JSON::cJSON_SafeGetStringValue(content, "FilePath");
                cJSON_Delete(content);

                QFileInfo info(filePath.data());
                if(info.exists() && info.isFile())
                {
                    sendFileMessage(uid, message.ChatType, filePath.data(), message.FileName.data(),
                                                      message.FileSize.data(), message.MsgId);
                }
                else
                {
                    if(item)
                        item->onDisconnected();
                    error_log("{0} message resend failed reason: file not exists", message.MsgId);
                    QtMessageBox::warning(this, tr("警告"), tr("原文件不存在, 消息重发失败!"));
                }
                break;
            }
            case QTalk::Entity::MessageTypeImageNew:
            case QTalk::Entity::MessageTypePhoto:
            {
                if(_mapSession[uid])
                {
                    QString id = _mapSession[uid]->conversionId();
                    _mapSession[uid]->sendEmoticonMessage(id, message.Content.data(), false, message.MsgId);
                }
                break;
            }
            default:
                break;
        }
    }
}

void dealImage(QString& strRet, QString imagePath, const QString& imageLink)
{
    QString srcImgPath = QTalk::GetSrcImagePathByUrl(imageLink.toStdString()).data();
    QFileInfo srcInfo(srcImgPath);
    if(srcInfo.exists() && srcInfo.isFile())
        imagePath = srcImgPath;

    // load image
    std::string realSuffix = QTalk::qimage::instance().getRealImageSuffix(imagePath).toStdString();
    QImage image;
    image.load(imagePath, realSuffix.data());
    int w = image.width();
    int h = image.height();
    // 移动原图处理
    if(!imageLink.isEmpty() && !imagePath.isEmpty() && !srcInfo.exists())
    {
        QFile::copy(imagePath, srcImgPath);
    }
    strRet.append(QString("[obj type=\"image\" value=\"%3\" width=%2 height=%1]").arg(h).arg(w).arg(imageLink));
}

QString ChatViewMainPanel::getRealText(const QString &json, const QString& msgId, bool & success, std::map<std::string, std::string>& mapAt) {

    success = false;
    QString strRet;
    //
    auto document = QJsonDocument::fromJson(json.toUtf8());
    if(document.isNull()){

    }
    else
    {
        QJsonArray array = document.array();
        for(auto && i : array)
        {
            QJsonObject obj = i.toObject();
            int key = obj.value("key").toInt();
            QString value = obj.value("value").toString();
            switch (key)
            {
                case Type_Text:
                {
                    strRet.append(value);
                    break;
                }
                case Type_Image:
                {
                    int index = 3;
                    //
                    QFileInfo info(value);
                    if(!info.exists() || info.isDir())
                        index = -1;

                    //
                    QString imageLink = obj.value("info").toString();
                    if(!imageLink.isEmpty())
                    {
                        dealImage(strRet, value, imageLink);
                        break;
                    }
                    //
                    while (index > 0)
                    {
                        imageLink = _pMessageManager->getNetFilePath(
                                std::string((const char *) value.toLocal8Bit())).data();
                        if(!imageLink.isEmpty())
                        {
                            dealImage(strRet, value, imageLink);
                            break;
                        }
                        index--;
                    }
                    //
                    if(index <= 0 )
                    {
                        success = false;
//                        emit _pMainWgt->sgSendFailed(msgId);
                        emit sgShowInfoMessageBox(tr("消息发送失败"));
                        return QString();
                    }
                    break;
                }
                case Type_At:
                {
                    QString xmppId = obj.value("info").toString();
                    mapAt[xmppId.toStdString()] = value.toStdString();
                    strRet.append(QString("@%1 ").arg(value));
                    break;
                }
                case Type_Url:
                {
                    strRet.append(QString("[obj type=\"url\" value=\"%1\"]").arg(value));
                    break;
                }
                case Type_Invalid:
                default:
                    break;
            }
        }
    }

    //先替换全角的@
    strRet.replace(tr("＠"), "@");
//    替换非法字符
    QRegExp regExp("[\\x00-\\x08\\x0b-\\x0c\\x0e-\\x1f]");
    strRet.replace(regExp, "");

//     去掉最后一个换行
    while (strRet.right(1) == "\n") {
        strRet = strRet.mid(0, strRet.size() - 1);
    }
    success = true;
    return strRet;
}

void ChatViewMainPanel::scanQRcCodeImage(const QString &imgPath) {

    if(_pQRcode && _pQRcode->_pScanQRcode)
    {
        QPixmap pix = QTalk::qimage::instance().loadPixmap(imgPath, false);
        _pQRcode->_pScanQRcode->scanPixmap(pix, true);
    }
}


void ChatViewMainPanel::preSendMessage(const QTalk::Entity::UID& uid,
                                       int chatType,
                                       int msgType, const QString& info, const std::string& messageId)
{
    if (g_pMainPanel && g_pMainPanel->getMessageManager()) {
        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;

        QTalk::Entity::ImMessageInfo message;
        message.ChatType = chatType;
        message.MsgId = messageId;
        message.To = uid.usrId();
        message.RealJid = uid.realId();
        message.From = g_pMainPanel->getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = msgType;
        message.Content = "";
        message.ExtendedInfo = info.toStdString();
        message.Direction = QTalk::Entity::MessageDirectionSent;
        message.UserName = Platform::instance().getSelfName();

        if(_mapSession.contains(uid) && _mapSession[uid])
            emit _mapSession[uid]->showMessage(message, false);

        emit g_pMainPanel->sgUserSendMessage();
        // 消息处理
        g_pMainPanel->getMessageManager()->preSendMessage(message);
    }
}

/**
  * @函数名   sendTextMessage
  * @功能描述 发送文本消息
  * @参数
  * @author   cc
  * @date     2018/09/19
  */
void ChatViewMainPanel::sendTextMessage(const QTalk::Entity::UID& uid,
                                        int chatType,
                                        const std::string &text, const std::map<std::string, std::string> &mapAt
        , const std::string& messageId) {
    //
    if(text.empty())
    {
        emit g_pMainPanel->sgShowInfoMessageBox(tr("无效的空消息"));
        return;
    }

    if (g_pMainPanel && g_pMainPanel->getMessageManager()) {

        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;
        std::string msgId = messageId.empty() ? QTalk::utils::getMessageId() : messageId;

        QTalk::Entity::ImMessageInfo message;
        S_Message e;

        message.ChatType = chatType;
        message.To = uid.usrId();
        message.RealJid = uid.realId();
        e.userId = uid.usrId();

        message.MsgId = msgId;
        message.From = g_pMainPanel->getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = mapAt.empty() ? QTalk::Entity::MessageTypeText : QTalk::Entity::MessageTypeGroupAt;
        message.Content = text;

        message.Direction = QTalk::Entity::MessageDirectionSent;
        message.UserName = Platform::instance().getSelfName();

        //
        e.message = message;
        e.chatType = message.ChatType;
        e.time = send_time;

        // 发送消息
        std::string backupInfo;
        if(!mapAt.empty())
        {
            cJSON *objs = cJSON_CreateArray();
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(obj, "type", 10001);
            cJSON *datas = cJSON_CreateArray();
            auto it = mapAt.begin();
            for (; it != mapAt.end(); it++) {
                cJSON *data = cJSON_CreateObject();
                cJSON_AddStringToObject(data, "jid", it->first.c_str());
                cJSON_AddStringToObject(data, "text", it->second.c_str());
                cJSON_AddItemToArray(datas, data);
            }
            cJSON_AddItemToObject(obj, "data", datas);
            cJSON_AddItemToArray(objs, obj);
            backupInfo = QTalk::JSON::cJSON_to_string(objs);
            cJSON_Delete(objs);
        }
        //
        message.BackupInfo = backupInfo;

        emit g_pMainPanel->sgUserSendMessage();
        g_pMainPanel->getMessageManager()->sendMessage(e);

        // 显示消息
        {
            if(_mapSession.contains(uid) && _mapSession[uid])
                    emit _mapSession[uid]->showMessage(message, false);
        }
    }
}

void
ChatViewMainPanel::sendCodeMessage(const QTalk::Entity::UID& uid,
        int chatType,
        const std::string &text,
        const std::string &codeType,
        const std::string &codeLanguage,
        const std::string& messageId) {

    if (g_pMainPanel && g_pMainPanel->getMessageManager()) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "CodeDecode", "");
        cJSON_AddStringToObject(obj, "CodeType", codeLanguage.data());
        cJSON_AddStringToObject(obj, "CodeStyle", codeType.data());
        cJSON_AddStringToObject(obj, "Code", text.data());
        std::string extenInfo = QTalk::JSON::cJSON_to_string(obj);
        cJSON_Delete(obj);

        // 发送消息
        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;
        std::string msgId = messageId.empty() ? utils::getMessageId() : messageId;
        QTalk::Entity::ImMessageInfo message;
        message.ChatType = chatType;
        message.MsgId = msgId;
        message.To = uid.usrId();
        message.RealJid = uid.realId();
        message.From = g_pMainPanel->getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = QTalk::Entity::MessageTypeSourceCode;
        message.Content = text;
        message.ExtendedInfo = extenInfo;
        message.Direction = QTalk::Entity::MessageDirectionSent;
        message.UserName = Platform::instance().getSelfName();

        S_Message e;
        e.message = message;
        e.userId = uid.usrId();
        e.time = send_time;
        e.chatType = chatType;

        emit g_pMainPanel->sgUserSendMessage();
        g_pMainPanel->getMessageManager()->sendMessage(e);

        if(_mapSession.contains(uid) && _mapSession[uid] && messageId.empty())
            emit _mapSession[uid]->showMessage(message, false);
    }
}

/**
  * @函数名   sendFileMessage
  * @功能描述 发送文件消息
  * @参数
  * @author   cc
  * @date     2018/10/16
  * //为了处理windows字符集导致的显示问题 发送消息时FilePath 用local字符集 记录用utf8字符集
  */
void ChatViewMainPanel::sendFileMessage(const QTalk::Entity::UID& uid,
                                        int chatType,
                                        const QString &filePath,
                                        const QString &fileName,
                                        const QString &fileSize,
                                        const std::string& messageId) {

    QString tmp_file_path(filePath);

    // 上传文件
    if (g_pMainPanel && g_pMainPanel->getMessageManager()) {
        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;
        std::string msgId = messageId.empty() ? utils::getMessageId() : messageId;
        // 本地显示
        QTalk::Entity::ImMessageInfo message;
        message.ChatType = chatType;
        message.MsgId = msgId;
        message.To = uid.usrId();
        message.RealJid = uid.realId();
        message.From = g_pMainPanel->getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = QTalk::Entity::MessageTypeFile;
        message.Direction = QTalk::Entity::MessageDirectionSent;
        message.UserName = Platform::instance().getSelfName();
        message.FileName = fileName.toStdString();
        message.FileSize = fileSize.toStdString();
        QFuture<void> future = QtConcurrent::run([tmp_file_path, &message](){
            message.FileMd5 = QTalk::utils::getFileMd5(tmp_file_path.toLocal8Bit().data());
        });

        while(!future.isFinished())
            QApplication::processEvents(QEventLoop::AllEvents, 100);

#ifdef _WINDOWS
        if(message.FileMd5.empty())
        {
            QFileInfo fileInfo(tmp_file_path);
            if ( fileInfo.exists() && fileInfo.size() < 200 * 1024 * 1024) {
                tmp_file_path = QString("%1/%2.%3").arg(Platform::instance().getAppdataRoamingUserPath().data()).arg(QDateTime::currentMSecsSinceEpoch()).arg(fileInfo.completeSuffix());
                QFile::copy(filePath, tmp_file_path);
                message.FileMd5 = QTalk::utils::getFileMd5(tmp_file_path.toLocal8Bit().data());
            }
            else {
                emit g_pMainPanel->sgShowInfoMessageBox(tr("文件路径问题，导致文件上传失败，请确认!"));
                return;
            }
        }
#endif // _WINDOWS
        {
            cJSON *content = cJSON_CreateObject();
            cJSON_AddStringToObject(content, "FileName", message.FileName.c_str());
            cJSON_AddStringToObject(content, "FileSize", message.FileSize.c_str());
            cJSON_AddStringToObject(content, "FILEMD5", message.FileMd5.c_str());
            cJSON_AddStringToObject(content, "FilePath", filePath.toStdString().data());
            message.ExtendedInfo = QTalk::JSON::cJSON_to_string(content);
            cJSON_Delete(content);
        }
        // 创建软链
        g_pMainPanel->makeFileLink(filePath, message.FileMd5.data());
        // 显示消息
        if(_mapSession.contains(uid) && _mapSession[uid] &&  messageId.empty())
        {
            emit _mapSession[uid]->showMessage(message, false);
            g_pMainPanel->getMessageManager()->preSendMessage(message);
        }
        // 上传文件 发送消息

        std::thread([this, uid, chatType, tmp_file_path, filePath, message, send_time, msgId]() {

            S_Message e;
            e.message = message;
            e.userId = uid.usrId();
            e.time = send_time;
            e.chatType = chatType;
            // get url
            std::string url = g_pMainPanel->getMessageManager()->uploadFile(tmp_file_path.toLocal8Bit().data(), true, msgId);

            e.message.FileUrl = url;
            //
            if (filePath != tmp_file_path)
            {
                QFile::remove(tmp_file_path);
            }

            if (!url.empty()) {

                cJSON *content = cJSON_CreateObject();
                cJSON_AddStringToObject(content, "FILEID", message.MsgId.c_str());
                cJSON_AddStringToObject(content, "FileName", message.FileName.c_str());
                cJSON_AddStringToObject(content, "FileSize", message.FileSize.c_str());
                cJSON_AddStringToObject(content, "FILEMD5", message.FileMd5.c_str());
                cJSON_AddStringToObject(content, "HttpUrl", url.c_str());
                std::string strContent = QTalk::JSON::cJSON_to_string(content);
                e.message.Content = strContent;
                e.message.ExtendedInfo = strContent;
                cJSON_Delete(content);

                emit g_pMainPanel->sgUserSendMessage();
                g_pMainPanel->getMessageManager()->sendMessage(e);
            }
            else
            {
                if(_mapSession.contains(uid) && _mapSession[uid] && _mapSession[uid]->_pChatMainWgt)
                        emit _mapSession[uid]->_pChatMainWgt->sgSendFailed(msgId.data());
                emit g_pMainPanel->sgShowInfoMessageBox(tr("文件上传失败!"));
            }
        }).detach();
    }
}

/**
 *
 * @param uid
 * @param chatType
 */
void ChatViewMainPanel::sendShockMessage(const QTalk::Entity::UID& uid,
                                         int chatType)
{
    if (g_pMainPanel && g_pMainPanel->getMessageManager()) {
        // 发送消息
        long long send_time =
                QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;
        std::string msgId = utils::getMessageId();

        QTalk::Entity::ImMessageInfo message;
        message.ChatType = chatType;
        message.MsgId = msgId;
        message.To = uid.usrId();
        message.RealJid = uid.realId();
        message.From = g_pMainPanel->getSelfUserId();
        message.LastUpdateTime = send_time;
        message.Type = QTalk::Entity::MessageTypeShock;
        message.Content = "[窗口抖动]";
        message.Direction = QTalk::Entity::MessageDirectionSent;
        message.UserName = Platform::instance().getSelfName();

        S_Message e;
        e.message = message;
        e.userId = uid.usrId();
        e.time = send_time;
        e.chatType = chatType;

        emit g_pMainPanel->sgUserSendMessage();
        g_pMainPanel->getMessageManager()->sendMessage(e);

        if(_mapSession.contains(uid) && _mapSession[uid])
            emit _mapSession[uid]->showMessage(message, false);
    }
}

//
void ChatViewMainPanel::sendStartAudioVideoMessage(const QTalk::Entity::UID &uid, bool isVideo) {

//    QtConcurrent::run([this, isVideo, uid](){
//        if(_pMessageManager && _pAudioVideoManager && !_pAudioVideoManager->isBusy())
//        {
//            QString json = "{\"type\":\"create\"}";
//            _pMessageManager->sendWebRtcCommand(isVideo ? QTalk::Entity::WebRTC_MsgType_VideoCall : QTalk::Entity::WebRTC_MsgType_AudioCall
//                    , json.toStdString(), uid.usrId());
//        }
//
//    });
}

void ChatViewMainPanel::sendAudioVideoMessage(const QTalk::Entity::UID& uid,
                                              int chatType, bool isVideo, const QString& json) {
    long long send_time =
            QDateTime::currentDateTime().toMSecsSinceEpoch() - Platform::instance().getServerDiffTime() * 1000;
    std::string msgId = utils::getMessageId();
    QTalk::Entity::ImMessageInfo message;
    message.ChatType = chatType;
    message.MsgId = msgId;
    message.To = uid.usrId();
    message.RealJid = uid.realId();
    message.From = g_pMainPanel->getSelfUserId();
    message.LastUpdateTime = send_time;
    message.Content ="当前客户端不支持实时视频";
    if(chatType == QTalk::Enum::TwoPersonChat)
    {
        if(isVideo)
            message.Type = QTalk::Entity::WebRTC_MsgType_VideoCall;
        else
        {
            message.Content ="当前客户端不支持实时音频";
            message.Type = QTalk::Entity::WebRTC_MsgType_AudioCall;
        }
    }
    else if(chatType == QTalk::Enum::GroupChat)
        message.Type = QTalk::Entity::WebRTC_MsgType_Video_Group;
    else
        return;

    message.Direction = QTalk::Entity::MessageDirectionSent;
    message.UserName = Platform::instance().getSelfName();
    message.ExtendedInfo = json.toStdString();

    S_Message e;
    e.message = message;
    e.userId = uid.usrId();
    e.time = send_time;
    e.chatType = chatType;

    emit g_pMainPanel->sgUserSendMessage();
    g_pMainPanel->getMessageManager()->sendMessage(e);
    if(_mapSession.contains(uid) && _mapSession[uid])
        emit _mapSession[uid]->showMessage(message, false);
}

void ChatViewMainPanel::postInterface(const std::string &url, const std::string &params)
{
    std::thread([this, url, params](){
        if(_pMessageManager)
            _pMessageManager->postInterface(url, params);
    }).detach();
}

void ChatViewMainPanel::sendGetRequest(const std::string &url) {
    std::thread([this, url](){
        if(_pMessageManager)
            _pMessageManager->sendGetRequest(url);
    }).detach();
}

void ChatViewMainPanel::updateMessageExtendInfo(const std::string &msgId, const string &info) {
    std::thread([this, msgId, info](){
        if(_pMessageManager)
            _pMessageManager->updateMessageExtendInfo(msgId, info);
    }).detach();
}

//
void ChatViewMainPanel::onRecvWebRtcCommand(int msgType, const std::string &peerId, const std::string &cmd, bool isCarbon) {

    qDebug() << cmd.data();
    QJsonDocument document = QJsonDocument::fromJson(cmd.data());
    if(document.isNull())
    {

    }
    else {
        auto obj = document.object();

        QString type = obj.value("type").toString();
        if((type == "create" || type == "busy" /*|| type == "canceled" || type == "close" || type == "deny"*/) && isCarbon)
            return;

        if (type == "create") {
#ifdef _WINDOWS
#ifdef PLATFORM_WIN32
        QtMessageBox::information(g_pMainPanel, tr("提醒"), tr("暂不支持此功能!"));
        return;
#endif
#endif
//            onRecvVideo(peerId);
            std::string selfId = Platform::instance().getSelfUserId();
            emit sgShowVidioWnd(QTalk::Entity::JID(peerId.data()).barename(), selfId,
                    WebRTC_MsgType_VideoCall == msgType);

        } else {
            if (_pAudioVideoManager)
            {
                if(type == "pickup" && isCarbon)
                {
                    _pAudioVideoManager->onCloseAudioVideo(peerId);
                }
                if(type == "deny" && isCarbon)
                {
                    _pAudioVideoManager->onCloseAudioVideo(peerId);
                }
                else
                {
                    //
                    _pAudioVideoManager->onRecvWebRtcCommand(peerId.data(), cmd.data());
                    //
                    if((type == "cancel" || type == "deny" || type == "timeout") && _pAudioVideoManager->isCall(peerId))
                    {
                        QString njson = QString("{\"type\":\"%1\", \"time\": 0, \"desc\":\"\"}").arg(type);
                        bool isVideo = _pAudioVideoManager->isVideo(peerId);
                        sendAudioVideoMessage(QTalk::Entity::UID(peerId), QTalk::Enum::TwoPersonChat, isVideo, njson);
                    }
                }
            }
        }
    }
}

void ChatViewMainPanel::start2Talk_old(const std::string &peerId, bool isVideo, bool isCall) {

    if(_pAudioVideoManager)
    {
        QFuture<std::string> future = QtConcurrent::run([this]() -> std::string{
            QString url = QString("%1/rtc?username=%2")
                    .arg(NavigationManager::instance().getVideoUrl().data())
                    .arg(Platform::instance().getClientAuthKey().data());
            return getMessageManager()->sendGetRequest(url.toStdString());
        });
        while(!future.isFinished())
            QApplication::processEvents(QEventLoop::AllEvents, 100);

        std::string data = future.result();
        _pAudioVideoManager->start2Talk_old(data.data(), peerId, isVideo, isCall);
    }
}

//
void ChatViewMainPanel::onSendSignal(const QString &json, const QString& id) {
    QtConcurrent::run([this, json, id](){

        if(_pMessageManager && _pAudioVideoManager)
        {
            _pMessageManager->sendWebRtcCommand(
                    _pAudioVideoManager->isVideo(id.toStdString()) ?
                    QTalk::Entity::WebRTC_MsgType_VideoCall :
                    QTalk::Entity::WebRTC_MsgType_AudioCall, json.toStdString(), id.toStdString());
        }

        auto document = QJsonDocument::fromJson(json.toUtf8());
        if(document.isNull())
        {

        }
        else {
            auto obj = document.object();
            QString type = obj.value("type").toString();
            if((type == "cancel" || type == "deny" || type == "timeout") && _pAudioVideoManager->isCall(id.toStdString()))
            {
                QString njson = QString("{\"type\":\"%1\", \"time\": 0, \"desc\":\"\"}").arg(type);
                bool isVideo = _pAudioVideoManager->isVideo(id.toStdString());
                sendAudioVideoMessage(QTalk::Entity::UID(id), QTalk::Enum::TwoPersonChat, isVideo, njson);
            }
        }
    });
}

//
void ChatViewMainPanel::startGroupTalk(const QString &id, const QString &name) {
    if(_pAudioVideoManager)
    {
        _pAudioVideoManager->startGroupTalk(id, name);
    }
}


void ChatViewMainPanel::getUserMedal(const std::string& xmppId, std::set<QTalk::StUserMedal>& medal) {

    if(_user_medals.find(xmppId) != _user_medals.end())
    {
        medal = _user_medals[xmppId];
        return;
    }
    if(_pMessageManager)
    {
        _pMessageManager->getUserMedal(xmppId, medal);
        _user_medals[xmppId] = medal;
    }
}

void ChatViewMainPanel::onUserMadelChanged(const std::vector<QTalk::Entity::ImUserStatusMedal> &userMedals) {

    std::set<std::string> changedUser;
    for(const auto& medal : userMedals)
    {
        std::string xmppId = medal.userId + "@" + medal.host;
        if(_user_medals.find(xmppId) != _user_medals.end())
        {
            auto &curUserMedal = _user_medals[xmppId];

            auto itFind = std::find_if(curUserMedal.begin(), curUserMedal.end(), [medal](const QTalk::StUserMedal& m){
                return m.medalId == medal.medalId;
            });

            if(itFind != curUserMedal.end())
                curUserMedal.erase(itFind);

            if(medal.medalStatus != 0)
                curUserMedal.insert(QTalk::StUserMedal(medal.medalId, medal.medalStatus));

            changedUser.insert(xmppId);
        }
    }
    //
    for(const auto& wgt : _mapSession)
    {
        if(wgt && wgt->_chatType == QTalk::Enum::GroupChat)
            wgt->_pChatMainWgt->onUserMedalChanged(changedUser);
    }
}
