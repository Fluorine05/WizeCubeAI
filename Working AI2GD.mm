// ────────────────────────────────────────────────
//  AI‑to‑GD  ·  main.cpp  (April 2025, MSVC‑safe)
// ────────────────────────────────────────────────
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>

#include <matjson.hpp>
#include <matjson/std.hpp>

#include <cocos-ext.h>
#include <sstream>
#include <string>

using namespace geode::prelude;
using namespace cocos2d;
using namespace cocos2d::extension;
using namespace web;

// ───────── helpers ─────────
static CCSprite* safeCreateSprite(const char* name) {
    auto s = CCSprite::create(name);
    if (!s) log::error("❌  failed to load {}", name);
    return s;
}
static CCSprite* safeCreateFallback(const char* name) {
    if (auto s = safeCreateSprite(name)) return s;

    static bool sheetLoaded = false;
    if (!sheetLoaded) {
        CCSpriteFrameCache::sharedSpriteFrameCache()
            ->addSpriteFramesWithFile("GJ_GameSheet01.plist");
        sheetLoaded = true;
    }
    return CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png");
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
    out << line; return out.str();
}

// ───────── ChatLogLayer ─────────
class ChatLogLayer : public CCLayer {
    CCScrollView*         m_scroll{};
    CCLayer*              m_container{};
public:
    static ChatLogLayer* create(const std::vector<std::string>& hist) {
        auto p = new ChatLogLayer;
        if (p && p->init(hist)) { p->autorelease(); return p; }
        CC_SAFE_DELETE(p); return nullptr;
    }
    bool init(const std::vector<std::string>& history) {
        if (!CCLayer::init()) return false;
        setKeypadEnabled(true);

        auto win = CCDirector::sharedDirector()->getWinSize();
        if (auto bg = safeCreateSprite("ai_background.png"_spr)) {
            bg->setAnchorPoint({.5f,.5f});
            bg->setPosition({win.width/2,win.height/2});
            bg->setScaleX(win.width  / bg->getContentSize().width);
            bg->setScaleY(win.height / bg->getContentSize().height);
            bg->setZOrder(-1); addChild(bg);
        }

        auto title = CCLabelBMFont::create("AI Chats", "bigFont.fnt");
        title->setPosition({win.width/2, win.height-40}); addChild(title);

        m_container = CCLayer::create();
        const float lineH = 30.f;
        for (size_t i = 0; i < history.size(); ++i) {
            auto lbl = CCLabelBMFont::create(history[i].c_str(),"bigFont.fnt");
            lbl->setScale(.4f);
            lbl->setAnchorPoint({0,1});
            lbl->setColor({255,215,0});
            lbl->setPosition({0, -lineH*static_cast<float>(i)});
            m_container->addChild(lbl);
        }
        m_container->setContentSize({win.width-60, history.size()*lineH});
        m_scroll = CCScrollView::create(
            {win.width-60, win.height-120}, m_container);
        m_scroll->setPosition({30,60});
        m_scroll->setDirection(kCCScrollViewDirectionVertical);
        addChild(m_scroll);
        m_scroll->setContentOffset(m_scroll->minContainerOffset());

        auto closeSpr =
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto back = CCMenuItemSpriteExtra::create(
            closeSpr, this, menu_selector(ChatLogLayer::onBack));
        back->setScale(.5f);
        auto menu = CCMenu::create(back,nullptr);
        menu->setPosition({30,win.height-40}); addChild(menu);
        return true;
    }
    void onBack(CCObject*) { CCDirector::sharedDirector()->popScene(); }
    void keyBackClicked() override { onBack(nullptr); }
};

// ───────── AIScene ─────────
class AIScene : public CCLayer {
    CCLabelTTF*            m_answer{};
    TextInput*             m_input{};
    std::vector<std::string> m_history;
    EventListener<WebTask> m_listener;
public:
    static AIScene* create() {
        auto p = new AIScene; if (p && p->init()) { p->autorelease(); return p; }
        CC_SAFE_DELETE(p); return nullptr;
    }
    bool init() override {
        if (!CCLayer::init()) return false; setKeypadEnabled(true);

        auto win = CCDirector::sharedDirector()->getWinSize();
        if (auto bg = safeCreateSprite("ai_background.png"_spr)) {
            bg->setAnchorPoint({.5f,.5f});
            bg->setPosition({win.width/2,win.height/2});
            bg->setScaleX(win.width/bg->getContentSize().width);
            bg->setScaleY(win.height/bg->getContentSize().height);
            bg->setZOrder(-1); addChild(bg);
        }

        auto title = CCLabelBMFont::create("Artificial Ideology","bigFont.fnt");
        title->setScale(.7f);
        title->setPosition({win.width/2, win.height-50});
        addChild(title);

        // close button
        auto closeSpr =
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto closeBtn = CCMenuItemSpriteExtra::create(
            closeSpr,this,menu_selector(AIScene::onClose));
        closeBtn->setScale(.5f);
        auto closeMenu = CCMenu::create(closeBtn,nullptr);
        closeMenu->setPosition({30, win.height-40}); addChild(closeMenu);

        // input
        m_input = TextInput::create(300.f,"Ask something...","bigFont.fnt");
        m_input->setMaxCharCount(100);
        m_input->setTextAlign(TextInputAlign::Center);
        m_input->setScale(1.2f);
        m_input->setPosition({win.width/2, win.height-130});
        addChild(m_input);

        // cube button
        auto cube = safeCreateFallback("AI_Cube.png"_spr);
        cube->setScale(.2f);
        cube->runAction(CCRepeatForever::create(
            CCSequence::create(CCMoveBy::create(1,{0,10}),
                               CCMoveBy::create(1,{0,-10}), nullptr)));
        auto cubeItem = CCMenuItemSpriteExtra::create(
            cube,this,menu_selector(AIScene::onSend));
        auto cubeMenu = CCMenu::create(cubeItem,nullptr);
        cubeMenu->setPosition({win.width/2, win.height-180}); addChild(cubeMenu);

        // answer
        m_answer = CCLabelTTF::create("", "Arial", 20,
            {win.width-80,160},
            kCCTextAlignmentCenter, kCCVerticalTextAlignmentTop);
        m_answer->setColor({255,215,0});
        m_answer->setScale(.7f);
        m_answer->setPosition({win.width/2, win.height-245});
        addChild(m_answer);

        // log button
        auto infoSpr =
            CCSprite::createWithSpriteFrameName("GJ_infoBtn_001.png");
        auto info = CCMenuItemSpriteExtra::create(
            infoSpr,this,menu_selector(AIScene::onLog));
        info->setScale(.5f);
        auto infoMenu = CCMenu::create(info,nullptr);
        infoMenu->setPosition({win.width-40,40}); addChild(infoMenu);

        return true;
    }

    // UI callbacks
    void onSend(CCObject*) {
        std::string q = m_input->getString();
        if (q.empty()) return;

        if (m_answer->getString()[0])
            m_history.emplace_back(std::string("AI: ") + m_answer->getString());
        m_history.emplace_back("You: " + q);

        m_input->setString(""); m_input->defocus();
        m_answer->setString("🤖 Thinking…");

        matjson::Value msg;
        msg["role"] = "user";
        msg["content"] = q;
        matjson::Value body;
        body["model"]    = "llama3-8b-8192";
        body["messages"] = std::vector<matjson::Value>{std::move(msg)};

        WebRequest req;
        req.header("Authorization",
                   "Bearer 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");
        req.header("Content-Type","application/json");
        req.bodyJSON(body);
        auto task = req.post(
            "https://api.groq.com/openai/v1/chat/completions");

        m_listener.bind([this](WebTask::Event* e){
            if (auto res = e->getValue(); res && res->ok()) {
                auto j = res->json().unwrap();
                auto content = j["choices"][0]["message"]["content"]
                                   .asString().unwrap();
                m_answer->setString(wrapText(content).c_str());
            } else m_answer->setString("Thinking...");
        });
        m_listener.setFilter(task);
    }
    void onLog(CCObject*) {
        auto scene = CCScene::create();
        scene->addChild(ChatLogLayer::create(m_history));
        CCDirector::sharedDirector()->pushScene(scene);
    }
    void onClose(CCObject*) { CCDirector::sharedDirector()->popScene(); }
    void keyBackClicked() override { onClose(nullptr); }
};

// ───────── MenuLayer hook ─────────
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto spr = CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png");
        auto btn = CCMenuItemSpriteExtra::create(spr,this,
                    menu_selector(MyMenuLayer::openAI));
        btn->setScale(.4f);
        btn->setID("ai-button"_spr);
        if (auto bar = getChildByID("bottom-menu")) {
            bar->addChild(btn); bar->updateLayout();
        }
        return true;
    }
    void openAI(CCObject*) {
        auto scene = CCScene::create();
        scene->addChild(AIScene::create());
        CCDirector::sharedDirector()->pushScene(scene);
    }
};
