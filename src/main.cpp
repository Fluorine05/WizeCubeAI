#include <cocos2d.h>
#include <cocos-ext.h>
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>
#include "HotkeyManager.hpp"
#include <matjson.hpp>
#include <matjson/std.hpp>
#include <sstream>
#include <string>
#include <fstream>
#include <ctime>

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;
using namespace web;
static std::string getLogPath() {
    return CCFileUtils::sharedFileUtils()->getWritablePath() + "ai_chat_log.txt";}
static void appendLog(const std::string& line) {
    std::ofstream out{ getLogPath(), std::ios::app };
    out << line << "\n";
}
static std::string wrapText(const std::string& in, size_t w = 48) {
    std::stringstream out, word; std::string line;
    std::istringstream ss(in);
    while (ss >> word.rdbuf()) {
        if (line.size() + word.str().size() > w) {
            out << line << '\n'; line.clear();
        }
        if (!line.empty()) line += ' ';
        line += word.str(); word.str("");
    }
    out << line;
    return out.str();
}
static CCSprite* safeCreateSprite(const char* name) {
    if (auto s = CCSprite::create(name)) return s;
    log::error("❌ failed to load {}", name);
    return nullptr;
}
static CCSprite* safeCreateFallback(const char* name) {
    if (auto s = safeCreateSprite(name)) return s;
    static bool sheet = false;
    if (!sheet) {
        CCSpriteFrameCache::sharedSpriteFrameCache()
            ->addSpriteFramesWithFile("GJ_GameSheet01.plist");
        sheet = true;
    }
    return CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png");
}
//Chat log
class ChatLogLayer : public CCLayer {
    CCScrollView* m_scroll{};
    CCLayer*      m_container{};
public:
    static ChatLogLayer* create(const std::vector<std::string>& hist) {
        auto p = new ChatLogLayer;
        if (p && p->init(hist)) { p->autorelease(); return p; }
        CC_SAFE_DELETE(p);
        return nullptr;
    }
    bool init(const std::vector<std::string>& history) {
        if (!CCLayer::init()) return false;
        setKeypadEnabled(true);

        auto win = CCDirector::sharedDirector()->getWinSize();
        // background
        if (auto bg = safeCreateSprite("ai_background.png"_spr)) {
            bg->setAnchorPoint({.5f,.5f});
            bg->setPosition({win.width/2,win.height/2});
            bg->setScaleX(win.width/bg->getContentSize().width);
            bg->setScaleY(win.height/bg->getContentSize().height);
            bg->setZOrder(-1);
            addChild(bg);
        }
        auto title = CCLabelBMFont::create("AI Chats","bigFont.fnt");
        title->setPosition({win.width/2,win.height-40});
        addChild(title);
        // container + lines
        m_container = CCLayer::create();
        const float lineH = 30.f;
        for (size_t i = 0; i < history.size(); ++i) {
            auto lbl = CCLabelBMFont::create(history[i].c_str(),"bigFont.fnt");
            lbl->setScale(.4f);
            lbl->setAnchorPoint({0,1});
            lbl->setColor(ccc3(255,215,0));
            lbl->setPosition({0, -lineH*float(i)});
            m_container->addChild(lbl);
        }
        m_container->setContentSize({win.width-60, history.size()*lineH});
        m_scroll = CCScrollView::create(
            CCSizeMake(win.width-60, win.height-120),
            m_container
        );
        m_scroll->setPosition({30,60});
        m_scroll->setDirection(kCCScrollViewDirectionVertical);
        addChild(m_scroll);
        m_scroll->setContentOffset(m_scroll->minContainerOffset());
        auto backS = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto back  = CCMenuItemSpriteExtra::create(backS, this, menu_selector(ChatLogLayer::onBack));
        back->setScale(.5f);
        auto m = CCMenu::create(back,nullptr);
        m->setPosition({30,win.height-40});
        addChild(m);

        return true;
    }
    void onBack(CCObject*)         { CCDirector::sharedDirector()->popScene(); }
    void keyBackClicked() override { onBack(nullptr); }
};

//mainScene
class AIScene : public CCLayer {
    CCLayer*               m_answerContainer{};
    CCScrollView*          m_scroll{};
    CCLabelTTF*            m_answerLabel{};
    TextInput*             m_input{};
    std::vector<std::string> m_history;
    EventListener<WebTask> m_listener;

public:
    static AIScene* create() {
        auto p = new AIScene;
        if (p && p->init()) { p->autorelease(); return p; }
        CC_SAFE_DELETE(p);
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        setKeypadEnabled(true);
        auto win = CCDirector::sharedDirector()->getWinSize();
        if (auto bg = safeCreateSprite("ai_background.png"_spr)) {
            bg->setAnchorPoint({.5f,.5f});
            bg->setPosition({win.width/2,win.height/2});
            bg->setScaleX(win.width/bg->getContentSize().width);
            bg->setScaleY(win.height/bg->getContentSize().height);
            bg->setZOrder(-1);
            addChild(bg);
        }
        auto title = CCLabelBMFont::create("Artificial Ideology","bigFont.fnt");
        title->setScale(.7f);
        title->setPosition({win.width/2,win.height-50});
        addChild(title);
        auto cs = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto cb = CCMenuItemSpriteExtra::create(cs,this,menu_selector(AIScene::onClose));
        cb->setScale(.5f);
        auto cm = CCMenu::create(cb,nullptr);
        cm->setPosition({30,win.height-40});
        addChild(cm);
        m_input = TextInput::create(300.f,"Ask something...","bigFont.fnt");
        m_input->setMaxCharCount(100);
        m_input->setTextAlign(TextInputAlign::Center);
        m_input->setScale(1.2f);
        m_input->setPosition({win.width/2,win.height-130});
        addChild(m_input);
        auto cube = safeCreateFallback("wize_cube.png"_spr);
        cube->setScale(.2f);
        cube->runAction(CCRepeatForever::create(
            CCSequence::create(
                CCMoveBy::create(1, {0,10}),
                CCMoveBy::create(1, {0,-10}),
                nullptr
            )
        ));
        auto ci = CCMenuItemSpriteExtra::create(cube,this,menu_selector(AIScene::onSend));
        auto cm2 = CCMenu::create(ci,nullptr);
        cm2->setPosition({win.width/2,win.height-175});
        addChild(cm2);

        m_answerContainer = CCLayer::create();
        m_answerContainer->setAnchorPoint({0,0});

        m_answerLabel = CCLabelTTF::create(
            "", "Arial", 18,
            CCSizeMake(win.width-80, 0),
            kCCTextAlignmentLeft, kCCVerticalTextAlignmentTop
        );
        m_answerLabel->setColor(ccc3(255,215,0));
        m_answerLabel->setAnchorPoint({0,1});
		m_answerLabel->setScale(0.6f); 
        m_answerContainer->addChild(m_answerLabel);

        m_answerLabel->setString(" ");
        float h = m_answerLabel->getContentSize().height * m_answerLabel->getScale();
        m_answerLabel->setString("");
        m_answerLabel->setPosition({0,h});
        m_answerContainer->setContentSize({win.width-80, h});

        m_scroll = CCScrollView::create(
            CCSizeMake(win.width-80, 140),
            m_answerContainer
        );
        m_scroll->setDirection(kCCScrollViewDirectionVertical);
        m_scroll->setPosition({140, win.height-185 -160});
        m_scroll->setContentOffset(m_scroll->minContainerOffset());
        addChild(m_scroll, 1);

		auto infoSpr =
            CCSprite::createWithSpriteFrameName("GJ_infoBtn_001.png");
        auto info = CCMenuItemSpriteExtra::create(
            infoSpr,this,menu_selector(AIScene::onLog));
        info->setScale(.5f);
        auto infoMenu = CCMenu::create(info,nullptr);
        infoMenu->setPosition({win.width-40,40}); addChild(infoMenu);


        return true;
    }

    void onSend(CCObject*) {
        auto q = m_input->getString();
        if (q.empty()) return;

        std::string youLine = "You: " + q;
        appendLog(youLine);
        m_history.push_back(youLine);

        m_input->setString("");
        m_input->defocus();
        updateAnswer("Gooner LOL");

        matjson::Value msg;    msg["role"]    = "user";
                              msg["content"] = q;
        matjson::Value body;   body["model"]    = "llama3-8b-8192";
                              body["messages"] = std::vector<matjson::Value>{ std::move(msg) };

        WebRequest req;
        req.header("Authorization","Bearer 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");
        req.header("Content-Type","application/json");
        req.bodyJSON(body);
        auto task = req.post("https://api.groq.com/openai/v1/chat/completions");

        m_listener.bind([this](WebTask::Event* e) {
            if (auto res = e->getValue(); res && res->ok()) {
                auto content = res->json().unwrap()
                                   ["choices"][0]["message"]["content"]
                                   .asString().unwrap();
				//thinking lol
                std::string aiLine = "AI: " + content;
                appendLog(aiLine);
                m_history.push_back(aiLine);

                updateAnswer(wrapText(content, 64));
            } else {
                updateAnswer("Gooning...");
            }
        });
        m_listener.setFilter(task);
    }

    void onLog(CCObject*) {
        auto sc = CCScene::create();
        sc->addChild(ChatLogLayer::create(m_history));
        CCDirector::sharedDirector()->pushScene(sc);
    }
    void onClose(CCObject*)        { CCDirector::sharedDirector()->popScene(); }
    void keyBackClicked() override { onClose(nullptr); }

private:
    void updateAnswer(const std::string& text) {
        m_answerLabel->setString(text.c_str());
        float newH = m_answerLabel->getContentSize().height * m_answerLabel->getScale();
        m_answerLabel->setPositionY(newH);
        m_answerContainer->setContentSize({ m_answerContainer->getContentSize().width, newH });
        m_scroll->setContentOffset(m_scroll->minContainerOffset());
    }
};

class $modify(MyMenuLayer, MenuLayer) {
	bool init() override {
		if (!MenuLayer::init()) return false;
		static bool s_shownHotkeyInfo = false;
		if (!s_shownHotkeyInfo) {
			this->runAction(CCSequence::create(
				CCDelayTime::create(0.1f),
				CCCallFunc::create(this, callfunc_selector(MyMenuLayer::showInfoPopup)),
				nullptr
			));
			s_shownHotkeyInfo = true;
		}

		HotkeyManager::get().setCallback([this]() {
			openAI(nullptr);
		});
		HotkeyManager::get().install();
	
		return true;
	
    }

    void openAI(CCObject*) {
        auto sc = CCScene::create();
        sc->addChild(AIScene::create());
        CCDirector::sharedDirector()->pushScene(sc);
    }

    void showInfoPopup() {
        FLAlertLayer::create(
            "Wize Cube AI",
            "To use the AI, press <cr>Control + Space</c>.\nEnjoy!",
            "OK"
        )->show();
    }

    ~MyMenuLayer() {
        HotkeyManager::get().uninstall();
    }
};
