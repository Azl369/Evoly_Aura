#include "aura/render.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace aura {
void Renderer::DrawWidget(ID2D1RenderTarget* target, HWND window, const WidgetView& view, float scale) {
    RECT client{}, screen{};
    GetClientRect(window,&client); GetWindowRect(window,&screen);
    float w=float(client.right)/scale,h=float(client.bottom)/scale;
    if (!widgetFonts_[0]) {
        const float sizes[]{11,13,15,19,30,70};
        for (size_t i=0;i<widgetFonts_.size();++i) {
            Check(text_->CreateTextFormat(L"Microsoft YaHei UI",nullptr,
                i==3?DWRITE_FONT_WEIGHT_SEMI_BOLD:i==5?DWRITE_FONT_WEIGHT_LIGHT:DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,sizes[i],L"zh-CN",&widgetFonts_[i]),"Widget font");
        }
    }
    const UINT32 accents[]{0x9cc9ff,0xc2c9d4,0x9bdbc1,0xf0bc92};
    const UINT32 bases[]{0x101e31,0x171c25,0x10271f,0x2b201e};
    UINT32 accent=accents[std::clamp(view.theme,0,3)],base=bases[std::clamp(view.theme,0,3)];
    constexpr UINT32 ink=0xf0f5fc,muted=0xa8bacd;
    target->Clear(D2D1::ColorF(0,0));
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ComPtr<ID2D1SolidColorBrush> brush;
    Check(target->CreateSolidColorBrush(D2D1::ColorF(base),&brush),"Widget brush");
    auto fill=[&](D2D1_RECT_F r,float radius,UINT32 color,float alpha=1.f) {
        brush->SetColor(D2D1::ColorF(color,alpha));
        target->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush.Get());
    };
    auto line=[&](float x1,float y1,float x2,float y2,UINT32 color,float alpha=1.f,float stroke=1.f) {
        brush->SetColor(D2D1::ColorF(color,alpha));
        target->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),brush.Get(),stroke);
    };
    auto text=[&](const std::wstring& value,int font,D2D1_RECT_F r,UINT32 color=0xf0f5fc,float alpha=1.f,
                  DWRITE_TEXT_ALIGNMENT alignment=DWRITE_TEXT_ALIGNMENT_LEADING) {
        auto* format=widgetFonts_[font].Get();
        format->SetTextAlignment(alignment);
        brush->SetColor(D2D1::ColorF(color,alpha));
        target->DrawTextW(value.c_str(),UINT32(value.size()),format,r,brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    };
    auto panel=D2D1::RoundedRect(D2D1::RectF(1,1,w-1,h-1),22,22);
    if (view.sampleWallpaper && !wallpaper_.image.pixels.empty()) {
        if (!wallpaperBitmap_) {
            const auto& image=wallpaper_.image;
            Check(target->CreateBitmap(D2D1::SizeU(image.width,image.height),image.pixels.data(),image.width*4,
                D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED)),
                &wallpaperBitmap_),"Widget wallpaper");
        }
        ComPtr<ID2D1BitmapBrush> image;
        Check(target->CreateBitmapBrush(wallpaperBitmap_.Get(),&image),"Widget wallpaper brush");
        image->SetTransform(D2D1::Matrix3x2F::Scale(
            float(wallpaper_.monitor.right-wallpaper_.monitor.left)/wallpaper_.image.width/scale,
            float(wallpaper_.monitor.bottom-wallpaper_.monitor.top)/wallpaper_.image.height/scale)*
            D2D1::Matrix3x2F::Translation(float(wallpaper_.monitor.left-screen.left)/scale,
                float(wallpaper_.monitor.top-screen.top)/scale));
        target->FillRoundedRectangle(panel,image.Get());
        brush->SetColor(D2D1::ColorF(base,view.opacity));
    } else brush->SetColor(D2D1::ColorF(base,view.sampleWallpaper?.94f:.58f+.38f*view.opacity));
    target->FillRoundedRectangle(panel,brush.Get());
    D2D1_GRADIENT_STOP stops[]{
        {0,D2D1::ColorF(0xffffff,.09f)},{1,D2D1::ColorF(0xffffff,.012f)}};
    ComPtr<ID2D1GradientStopCollection> collection;
    ComPtr<ID2D1LinearGradientBrush> sheen;
    Check(target->CreateGradientStopCollection(stops,2,&collection),"Glass sheen");
    Check(target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(
        D2D1::Point2F(0,0),D2D1::Point2F(w*.4f,h)),collection.Get(),&sheen),"Glass gradient");
    target->FillRoundedRectangle(panel,sheen.Get());
    brush->SetColor(D2D1::ColorF(view.editing?accent:0xe3f0ff,view.editing?.65f:.24f));
    target->DrawRoundedRectangle(panel,brush.Get(),1);
    line(24,1,w-24,1,0xffffff,.15f);

    auto areas=WidgetActions(view.kind,w,h);
    auto areaFor=[&](WidgetAction action) {
        for (const auto& area:areas) if (area.action==action) return area.bounds;
        return DipRect{};
    };
    auto rect=[](DipRect r) { return D2D1::RectF(r.x,r.y,r.x+r.width,r.y+r.height); };
    auto button=[&](WidgetAction action,const wchar_t* label,bool selected=false) {
        auto area=areaFor(action);
        bool hover=view.hovered==action;
        fill(rect(area),10,selected?accent:0xe1edff,selected?.19f:hover?.14f:.065f);
        if (selected || hover) {
            brush->SetColor(D2D1::ColorF(accent,selected?.4f:.2f));
            target->DrawRoundedRectangle(D2D1::RoundedRect(rect(area),10,10),brush.Get(),1);
        }
        text(label,1,D2D1::RectF(area.x+4,area.y+8,area.x+area.width-4,area.y+area.height),
            selected?accent:ink,1,DWRITE_TEXT_ALIGNMENT_CENTER);
    };
    auto heading=[&](const wchar_t* title,const wchar_t* eyebrow) {
        text(title,3,D2D1::RectF(24,20,w-24,49));
        if (eyebrow && *eyebrow) text(eyebrow,0,D2D1::RectF(w-108,27,w-24,45),muted,.9f,DWRITE_TEXT_ALIGNMENT_TRAILING);
    };
    auto icon=[&](int number,float x,float y,UINT32 color) {
        brush->SetColor(D2D1::ColorF(color));
        auto l=[&](float a,float b,float c,float d) { line(x+a,y+b,x+c,y+d,color,.95f,1.5f); };
        switch (number) {
        case 0:
            l(0,6,7,6);l(7,6,10,10);l(10,10,24,10);l(24,10,22,24);l(22,24,0,24);l(0,24,0,6);break;
        case 1:
            l(12,1,12,18);l(6,12,12,18);l(12,18,18,12);l(1,23,23,23);break;
        case 2:
            l(4,0,17,0);l(17,0,23,6);l(23,6,23,25);l(23,25,4,25);l(4,25,4,0);l(9,11,18,11);l(9,17,18,17);break;
        case 3:
            l(7,6,1,13);l(1,13,7,20);l(17,6,23,13);l(23,13,17,20);l(15,2,10,24);break;
        case 4:
            target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x+12,y+12),9,9),brush.Get(),1.5f);
            target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x+12,y+12),3,3),brush.Get(),1.5f);
            l(12,-1,12,3);l(12,21,12,25);l(-1,12,3,12);l(21,12,25,12);break;
        default:
            l(4,5,21,5);l(7,5,8,25);l(8,25,18,25);l(18,25,20,5);l(9,1,16,1);l(11,10,11,20);l(15,10,15,20);break;
        }
    };

    switch (view.kind) {
    case WidgetKind::toolbar:
        brush->SetColor(D2D1::ColorF(accent,.12f));
        target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(44,38),21,21),brush.Get());
        brush->SetColor(D2D1::ColorF(accent));
        target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(44,38),11,11),brush.Get(),1.5f);
        line(36,45,52,31,accent,1,1.5f);
        text(L"Evoly Aura",3,D2D1::RectF(80,16,280,42));
        text(view.editing?L"拖动标题区，右下角调整大小":view.temporaryReveal?L"临时唤起 · Esc 回到桌面":L"我的桌面 / 浮光",0,D2D1::RectF(80,46,450,64),muted);
        button(WidgetAction::editLayout,view.editing?L"完成编辑":L"编辑布局",view.editing);
        button(WidgetAction::toggleMode,view.desktop?L"切为悬浮模式":L"回到桌面模式");
        button(WidgetAction::resetLayout,L"重置布局");
        break;
    case WidgetKind::clock: {
        heading(L"此刻",L"LOCAL");
        WCHAR time[32]{},date[80]{};
        GetTimeFormatEx(LOCALE_NAME_INVARIANT,TIME_NOSECONDS,nullptr,L"HH:mm",time,32);
        GetDateFormatEx(L"zh-CN",0,nullptr,L"M月d日  dddd",date,80,nullptr);
        text(time,5,D2D1::RectF(20,62,w-20,160));
        text(date,2,D2D1::RectF(26,164,w-24,192),ink,.9f);
        fill(D2D1::RectF(26,h-27,32,h-21),3,accent);
        text(L"本地时间",0,D2D1::RectF(40,h-34,w-24,h-12),muted);
        break;
    }
    case WidgetKind::launcher: {
        heading(L"常用入口",L"QUICK ACCESS");
        const wchar_t* labels[]{L"文件",L"下载",L"文档",L"项目主页",L"系统设置",L"回收站"};
        for (size_t i=0;i<areas.size();++i) {
            auto a=areas[i];
            fill(rect(a.bounds),12,0xffffff,view.hovered==a.action?.13f:.045f);
            icon(int(i),a.bounds.x+16,a.bounds.y+12,accent);
            text(labels[i],1,D2D1::RectF(a.bounds.x+52,a.bounds.y+23,a.bounds.x+a.bounds.width-4,a.bounds.y+48),ink);
        }
        break;
    }
    case WidgetKind::focus: {
        heading(L"专注",L"FOCUS");
        float cx=w/2,cy=113,r=47;
        brush->SetColor(D2D1::ColorF(0xffffff,.09f));
        target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx,cy),r,r),brush.Get(),3);
        float fraction=std::clamp(view.focusSeconds/1500.f,0.f,1.f);
        brush->SetColor(D2D1::ColorF(accent));
        if (fraction>.999f) target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx,cy),r,r),brush.Get(),3);
        else if (fraction>.001f) {
            float angle=fraction*6.2831853f-1.5707963f;
            ComPtr<ID2D1PathGeometry> geometry;
            ComPtr<ID2D1GeometrySink> sink;
            Check(factory_->CreatePathGeometry(&geometry),"Focus path");
            Check(geometry->Open(&sink),"Focus path sink");
            sink->BeginFigure(D2D1::Point2F(cx,cy-r),D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(cx+r*std::cos(angle),cy+r*std::sin(angle)),
                D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,
                fraction>.5f?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL));
            sink->EndFigure(D2D1_FIGURE_END_OPEN); Check(sink->Close(),"Focus arc");
            target->DrawGeometry(geometry.Get(),brush.Get(),3);
        }
        wchar_t remaining[24]{};
        swprintf_s(remaining,L"%02d:%02d",view.focusSeconds/60,view.focusSeconds%60);
        text(remaining,4,D2D1::RectF(24,91,w-24,139),ink,1,DWRITE_TEXT_ALIGNMENT_CENTER);
        text(view.focusRunning?L"专注进行中":view.focusSeconds==0?L"本轮已完成":L"给一件事，留 25 分钟",0,
            D2D1::RectF(20,166,w-20,187),muted,1,DWRITE_TEXT_ALIGNMENT_CENTER);
        button(WidgetAction::focus,view.focusRunning?L"暂停":view.focusSeconds==0?L"再来一轮":L"开始专注",view.focusRunning);
        break;
    }
    case WidgetKind::notes:
        heading(L"便签",L"");
        button(WidgetAction::editNote,L"编辑");
        text(view.note.empty()?L"点「编辑」，写下第一条想法。":view.note,2,D2D1::RectF(26,72,w-26,h-47),ink,.96f);
        line(26,h-37,w-26,h-37,0xffffff,.09f);
        text(L"仅保存在这台电脑",0,D2D1::RectF(26,h-27,w-24,h-8),muted,.8f);
        break;
    case WidgetKind::checklist: {
        heading(L"桌面准备",L"GET STARTED");
        const wchar_t* labels[]{L"调整卡片，找到舒服的位置",L"在便签里留下一个想法",L"开启一段自己的专注时间"};
        int completed=0;
        for (int i=0;i<3;++i) {
            bool checked=(view.checkedTasks&(1<<i))!=0;
            completed+=checked?1:0;
            auto a=areas[size_t(i)];
            fill(rect(a.bounds),11,0xffffff,view.hovered==a.action?.1f:.035f);
            float y=a.bounds.y+13;
            fill(D2D1::RectF(38,y,54,y+16),5,checked?accent:0xffffff,checked?.25f:.1f);
            if (checked) {
                line(41,y+8,45,y+12,accent,1,1.5f);
                line(45,y+12,51,y+4,accent,1,1.5f);
            }
            text(labels[i],1,D2D1::RectF(68,a.bounds.y+11,w-36,a.bounds.y+38),checked?muted:ink,checked?.7f:1.f);
        }
        text(std::to_wstring(completed)+L" / 3 已完成",0,D2D1::RectF(26,h-27,w-24,h-8),muted);
        break;
    }
    case WidgetKind::appearance: {
        heading(L"外观",L"STYLE");
        const wchar_t* names[]{L"雾蓝",L"石墨",L"苔绿",L"暮光"};
        for (int i=0;i<4;++i) {
            auto a=areas[size_t(i)];
            fill(rect(a.bounds),10,accents[i],view.theme==i?.3f:view.hovered==a.action?.2f:.09f);
            if (view.theme==i) {
                brush->SetColor(D2D1::ColorF(accents[i],.65f));
                target->DrawRoundedRectangle(D2D1::RoundedRect(rect(a.bounds),10,10),brush.Get(),1);
            }
            brush->SetColor(D2D1::ColorF(accents[i]));
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(a.bounds.x+24,a.bounds.y+22),6,6),brush.Get());
            text(names[i],0,D2D1::RectF(a.bounds.x-1,123,a.bounds.x+a.bounds.width+1,144),muted,1,DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        text(L"玻璃浓度",0,D2D1::RectF(26,159,w-65,181),muted);
        text(std::to_wstring(int(view.opacity*100+.5f))+L"%",0,D2D1::RectF(w-70,159,w-26,181),ink,1,DWRITE_TEXT_ALIGNMENT_TRAILING);
        fill(D2D1::RectF(26,185,w-26,188),2,0xffffff,.09f);
        fill(D2D1::RectF(26,185,26+(w-52)*view.opacity,188),2,accent,.75f);
        button(WidgetAction::clearer,L"更通透");
        button(WidgetAction::frostier,L"更清晰");
        break;
    }
    }
    if (view.editing) {
        for (int i=0;i<3;++i) line(w-19+i*4,h-10,w-10,h-19+i*4,accent,.65f);
        if (view.kind!=WidgetKind::toolbar) {
            for (int i=0;i<3;++i) fill(D2D1::RectF(w/2-6+i*5,11,w/2-4+i*5,13),1,accent,.55f);
        }
    }
}
}
