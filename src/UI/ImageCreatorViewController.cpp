#include "UI/ImageCreatorViewController.hpp"

#include "UI/ImageFactoryFlowCoordinator.hpp"
#include "PluginConfig.hpp"
#include "Utils/UIUtils.hpp"
#include "Presenters/PresenterManager.hpp"
#include "Presenters/Presenter.hpp"
#include "HMUI/CurvedCanvasSettingsHelper.hpp"
#include "HMUI/Touchable.hpp"
#include "questui/shared/BeatSaberUI.hpp"
#include "UnityEngine/Rect.hpp"
#include "UnityEngine/RectOffset.hpp"
#include "UnityEngine/Graphics.hpp"

using namespace UnityEngine::UI;
using namespace UnityEngine;
using namespace QuestUI;
using namespace HMUI;

#define SetPreferredSize(identifier, width, height)                                         \
    auto layout##identifier = identifier->get_gameObject()->GetComponent<LayoutElement*>(); \
    if (!layout##identifier)                                                                \
        layout##identifier = identifier->get_gameObject()->AddComponent<LayoutElement*>();  \
    layout##identifier->set_preferredWidth(width);                                          \
    layout##identifier->set_preferredHeight(height)

#define SetWidth(identifier, w)                                         \
    auto width##identifier = identifier->get_gameObject()->GetComponent<LayoutElement*>(); \
    if (!width##identifier)                                                                \
        width##identifier = identifier->get_gameObject()->AddComponent<LayoutElement*>();  \
    width##identifier->set_preferredWidth(w);   

#define SetFits(identifier, vert, horiz) \
    auto fitter##identifier = identifier->get_gameObject()->GetComponent<ContentSizeFitter*>(); \
    if (!fitter##identifier)                                                                    \
        fitter##identifier = identifier->get_gameObject()->AddComponent<ContentSizeFitter*>();  \
    fitter##identifier->set_verticalFit(vert);                                                  \
    fitter##identifier->set_horizontalFit(horiz);

DEFINE_TYPE(ImageFactory::UI, ImageCreatorViewController);

namespace ImageFactory::UI {

    void ImageCreatorViewController::DidActivate(bool firstActivation,
                                         bool addedToHierarchy,
                                         bool screenSystemEnabling) {
        if (firstActivation) {
            if (!get_gameObject()) return;

            get_gameObject()->AddComponent<Touchable*>();

            if (editing) {
                image->Spawn(false);
            }
            
            image->Update(true);

            VerticalLayoutGroup* vertical = BeatSaberUI::CreateVerticalLayoutGroup(get_transform());
            SetFits(vertical, ContentSizeFitter::FitMode::PreferredSize, ContentSizeFitter::FitMode::PreferredSize);

            HorizontalLayoutGroup* horiz = BeatSaberUI::CreateHorizontalLayoutGroup(vertical->get_transform());
            SetFits(horiz, ContentSizeFitter::FitMode::PreferredSize, ContentSizeFitter::FitMode::PreferredSize);
            SetPreferredSize(horiz, 94, 8);

            BeatSaberUI::CreateStringSetting(horiz->get_transform(), "Name", image->name,
                [this](StringW s) { 
                    image->name = static_cast<std::string>(s);
            });

            
            VerticalLayoutGroup* settingsVert = BeatSaberUI::CreateVerticalLayoutGroup(vertical->get_transform());
            SetFits(settingsVert, ContentSizeFitter::FitMode::PreferredSize, ContentSizeFitter::FitMode::PreferredSize);
            SetWidth(horiz, 90);
            
            BeatSaberUI::CreateToggle(settingsVert->get_transform(), "Enabled", image->enabled, 
                [this](bool b) {
                    image->enabled = b;
                    image->Update(true);
            });

            BeatSaberUI::CreateIncrementSetting(settingsVert->get_transform(), "Scale X", 2, 0.1f, image->scaleX,
                [this](float f) {
                    image->scaleX = f;
                    image->Update(true);
            });

            BeatSaberUI::CreateIncrementSetting(settingsVert->get_transform(), "Scale Y", 2, 0.1f, image->scaleY,
                [this](float f) {
                    image->scaleY = f;
                    image->Update(true);
            });

            VerticalLayoutGroup* optionsVert = BeatSaberUI::CreateVerticalLayoutGroup(vertical->get_transform());
            optionsVert->set_padding(RectOffset::New_ctor(-2, -20, -2, -2));
            optionsVert->set_childAlignment(TMPro::TextAlignmentOptions::Center);

            SetFits(optionsVert, ContentSizeFitter::FitMode::PreferredSize, ContentSizeFitter::FitMode::PreferredSize);
            SetPreferredSize(optionsVert, 90, 34);

            Backgroundable* bg = optionsVert->get_gameObject()->AddComponent<Backgroundable*>();
            bg->ApplyBackgroundWithAlpha("round-rect-panel", 0.8f);

            std::vector<StringW> presOptions;

            for (int i = 0; i < PresenterManager::PRESENTERS.size(); i++) {
                presOptions.push_back(PresenterManager::PRESENTERS.at(i));
            }

            VerticalLayoutGroup* list = BeatSaberUI::CreateVerticalLayoutGroup(get_transform());
            list->get_rectTransform()->set_anchoredPosition({0.0f, -20.0f});
            list->set_spacing(2.0f);
            list->set_childForceExpandHeight(false);
            SetFits(list, ContentSizeFitter::FitMode::PreferredSize, ContentSizeFitter::FitMode::PreferredSize);
            SetPreferredSize(list, 85, 40);

            auto dropDown = BeatSaberUI::CreateDropdown(list->get_transform(), "Presentation Options", image->presentationoption, presOptions,
                [this, list](StringW s) {
                    image->presentationoption = static_cast<std::string>(s);

                    ResetOptions(list->get_transform());

                });

            presenter = PresenterManager::GetPresenter(image->presentationoption);
            options = presenter->GetUIElements(list->get_transform(), image);

            ImageFactoryFlowCoordinator* flow = Object::FindObjectsOfType<ImageFactoryFlowCoordinator*>().First();

            auto cancelButton = BeatSaberUI::CreateUIButton(this->get_transform(), "", {-22.0f, -38.0f}, {40.0f, 8.0f}, 
                [flow]() {
                    flow->ResetViews();
            });

            BeatSaberUI::CreateText(cancelButton->get_transform(), "CANCEL")->set_alignment(TMPro::TextAlignmentOptions::Center);

            auto saveButton = BeatSaberUI::CreateUIButton(this->get_transform(), "", {22.0f, -38.0f}, {40.0f, 8.0f},
                [this, flow]() {
                    GameObject* screen = image->screen;
                    auto localPosition = screen->get_transform()->get_localPosition();
                    auto eulerAngles = screen->get_transform()->get_rotation().get_eulerAngles();

                    image->x = localPosition.x;
                    image->y = localPosition.y;
                    image->z = localPosition.z;
                    image->angleX = eulerAngles.x;
                    image->angleY = eulerAngles.y;
                    image->angleZ = eulerAngles.z;

                    if (editing) {
                        Config::Update(image);

                        Object::Destroy(backUpImage);
                    } else {
                        Config::Add(image);
                    }
                    
                    PresenterManager::Parse(image, image->presentationoption);

                    hasSaved = true;

                    image->Update(false);
                    image->Despawn(false);

                    if (!editing) {
                        flow->imageEditingViewController->Refresh(image);
                    }

                    if (text) {
                        text->SetText(image->name);

                        if (!image->enabled) {
                            text->set_color(Color::get_red());
                        } else {
                            text->set_color(Color::get_green());
                        }
                    }
    
                    flow->ResetViews();
            });

            BeatSaberUI::CreateText(saveButton->get_transform(), "SAVE")->set_alignment(TMPro::TextAlignmentOptions::Center);
        }
    }

    void ImageCreatorViewController::DidDeactivate(bool removedFromHierarchy, bool screenSystemEnabling) {
        if (image) { 
            if (editing) {
                image->Despawn(false);
                backUpImage->Update(false);
                backUpImage->Despawn(false);
                backUpImage->Spawn(false);
                PresenterManager::ClearInfo(image);
                PresenterManager::Parse(backUpImage, backUpImage->presentationoption);
                hasSaved = true;
            }

            if (!hasSaved) {
                PresenterManager::ClearInfo(image);
                image->Despawn(false);
                Object::Destroy(image);
            }
        }

        PresenterManager::SpawnInMenu();
    }

    void ImageCreatorViewController::ResetOptions(Transform* list) {
        for (int i = 0; i < options.size(); i++) {
            Object::Destroy(options.at(i));
        }
        
        delete presenter;

        presenter = PresenterManager::GetPresenter(image->presentationoption);
        options = presenter->GetUIElements(list, image);
    }

    void ImageCreatorViewController::Initialize(StringW s) {
        path = s;

        GameObject* obj = GameObject::New_ctor(s);
        IFImage* image = obj->AddComponent<IFImage*>();
        image->ctor(BeatSaberUI::FileToSprite(static_cast<std::string>(s)), s);

        this->image = image;

        editing = false;
        hasSaved = false;
    }

    void ImageCreatorViewController::InitializeEditor(IFImage* image, TMPro::TextMeshProUGUI* text) {
        this->image = image;
        this->backUpImage = image;
        this->text = text;

        image->Spawn(false);

        editing = true;
        hasSaved = false;
    }
}