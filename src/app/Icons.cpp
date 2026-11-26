#include "Icons.hpp"

#include <cmath>

namespace blitzdj::app
{
    namespace
    {
        ImVec2 point(ImVec2 center, float size, float x, float y)
        {
            return ImVec2(center.x + (x - 12.0f) * size / 24.0f, center.y + (y - 12.0f) * size / 24.0f);
        }

        void arrowHead(ImDrawList* list, ImVec2 tip, ImVec2 direction, float length, ImU32 color)
        {
            const ImVec2 normal(-direction.y, direction.x);
            const ImVec2 base(tip.x - direction.x * length, tip.y - direction.y * length);

            list->AddTriangleFilled(
                tip,
                ImVec2(base.x + normal.x * length * 0.6f, base.y + normal.y * length * 0.6f),
                ImVec2(base.x - normal.x * length * 0.6f, base.y - normal.y * length * 0.6f),
                color
            );
        }
    }

    void drawIcon(ImDrawList* list, Icon icon, ImVec2 c, float s, ImU32 color)
    {
        const float stroke = std::fmax(1.5f, s / 12.0f);

        switch (icon)
        {
            case Icon::Play:
                list->AddTriangleFilled(point(c, s, 7, 4.5f), point(c, s, 7, 19.5f), point(c, s, 19.5f, 12), color);
                break;

            case Icon::Pause:
                list->AddRectFilled(point(c, s, 6, 5), point(c, s, 10, 19), color, s * 0.05f);
                list->AddRectFilled(point(c, s, 14, 5), point(c, s, 18, 19), color, s * 0.05f);
                break;

            case Icon::Next:
                list->AddTriangleFilled(point(c, s, 5, 6), point(c, s, 5, 18), point(c, s, 15, 12), color);
                list->AddRectFilled(point(c, s, 16, 6), point(c, s, 18.5f, 18), color);
                break;

            case Icon::Previous:
                list->AddTriangleFilled(point(c, s, 19, 6), point(c, s, 19, 18), point(c, s, 9, 12), color);
                list->AddRectFilled(point(c, s, 5.5f, 6), point(c, s, 8, 18), color);
                break;

            case Icon::Shuffle:
                list->AddLine(point(c, s, 4, 17), point(c, s, 17, 7), color, stroke);
                list->AddLine(point(c, s, 4, 7), point(c, s, 9, 10.5f), color, stroke);
                list->AddLine(point(c, s, 12.5f, 13.5f), point(c, s, 17, 17), color, stroke);
                arrowHead(list, point(c, s, 20.5f, 5), ImVec2(0.8f, -0.6f), s * 0.22f, color);
                arrowHead(list, point(c, s, 20.5f, 19), ImVec2(0.8f, 0.6f), s * 0.22f, color);
                break;

            case Icon::Repeat:
            case Icon::RepeatOne:
                list->AddLine(point(c, s, 5, 13), point(c, s, 5, 8), color, stroke);
                list->AddLine(point(c, s, 5, 8), point(c, s, 17, 8), color, stroke);
                arrowHead(list, point(c, s, 21, 8), ImVec2(1, 0), s * 0.24f, color);
                list->AddLine(point(c, s, 19, 11), point(c, s, 19, 16), color, stroke);
                list->AddLine(point(c, s, 19, 16), point(c, s, 7, 16), color, stroke);
                arrowHead(list, point(c, s, 3, 16), ImVec2(-1, 0), s * 0.24f, color);

                if (icon == Icon::RepeatOne)
                {
                    list->AddLine(point(c, s, 12, 10), point(c, s, 12, 14), color, stroke);
                }

                break;

            case Icon::Volume:
                list->AddRectFilled(point(c, s, 3, 9), point(c, s, 7, 15), color);
                list->AddTriangleFilled(point(c, s, 6, 9), point(c, s, 12, 4), point(c, s, 12, 20), color);
                list->AddTriangleFilled(point(c, s, 6, 9), point(c, s, 12, 20), point(c, s, 6, 15), color);
                list->PathArcTo(point(c, s, 12, 12), s * 4.5f / 24.0f, -0.9f, 0.9f, 12);
                list->PathStroke(color, 0, stroke);
                list->PathArcTo(point(c, s, 12, 12), s * 8.0f / 24.0f, -1.0f, 1.0f, 16);
                list->PathStroke(color, 0, stroke);
                break;

            case Icon::Search:
                list->AddCircle(point(c, s, 10, 10), s * 6.0f / 24.0f, color, 24, stroke * 1.2f);
                list->AddLine(point(c, s, 14.5f, 14.5f), point(c, s, 20, 20), color, stroke * 1.4f);
                break;

            case Icon::Bolt:
            {
                const ImVec2 points[] = {
                    point(c, s, 13, 2), point(c, s, 4, 14), point(c, s, 11, 14),
                    point(c, s, 10, 22), point(c, s, 20, 10), point(c, s, 13, 10)
                };

                list->AddConvexPolyFilled(points, 3, color);
                list->AddTriangleFilled(points[0], points[2], points[5], color);
                list->AddTriangleFilled(points[2], points[3], points[5], color);
                list->AddTriangleFilled(points[3], points[4], points[5], color);
                break;
            }

            case Icon::Music:
                list->AddCircleFilled(point(c, s, 9, 17), s * 3.5f / 24.0f, color, 20);
                list->AddRectFilled(point(c, s, 11, 4), point(c, s, 13, 17), color);
                list->AddRectFilled(point(c, s, 11, 4), point(c, s, 18, 7.5f), color);
                break;

            case Icon::Speaker:
                list->AddRect(point(c, s, 6, 2.5f), point(c, s, 18, 21.5f), color, s * 0.08f, 0, stroke);
                list->AddCircle(point(c, s, 12, 14.5f), s * 3.8f / 24.0f, color, 20, stroke);
                list->AddCircleFilled(point(c, s, 12, 7), s * 1.6f / 24.0f, color, 12);
                break;

            case Icon::Bluetooth:
            {
                const ImVec2 points[] = {
                    point(c, s, 6.5f, 7), point(c, s, 17, 16.5f), point(c, s, 12, 21),
                    point(c, s, 12, 3), point(c, s, 17, 7.5f), point(c, s, 6.5f, 17)
                };

                list->AddPolyline(points, 6, color, 0, stroke);
                break;
            }

            case Icon::Heart:
                list->AddCircleFilled(point(c, s, 8, 9), s * 4.6f / 24.0f, color, 20);
                list->AddCircleFilled(point(c, s, 16, 9), s * 4.6f / 24.0f, color, 20);
                list->AddTriangleFilled(point(c, s, 3.6f, 10.6f), point(c, s, 20.4f, 10.6f), point(c, s, 12, 20.5f), color);
                break;

            case Icon::QueueAdd:
                list->AddRectFilled(point(c, s, 3, 6), point(c, s, 14, 8), color);
                list->AddRectFilled(point(c, s, 3, 10), point(c, s, 14, 12), color);
                list->AddRectFilled(point(c, s, 3, 14), point(c, s, 10, 16), color);
                list->AddRectFilled(point(c, s, 16, 10), point(c, s, 18, 20), color);
                list->AddRectFilled(point(c, s, 12, 14), point(c, s, 22, 16), color);
                break;

            case Icon::Refresh:
                list->PathArcTo(point(c, s, 12, 12), s * 7.0f / 24.0f, 0.3f, 5.6f, 24);
                list->PathStroke(color, 0, stroke * 1.2f);
                arrowHead(list, point(c, s, 19.5f, 5.5f), ImVec2(0.5f, -0.85f), s * 0.26f, color);
                break;

            case Icon::Logout:
                list->AddLine(point(c, s, 12, 4), point(c, s, 5, 4), color, stroke);
                list->AddLine(point(c, s, 5, 4), point(c, s, 5, 20), color, stroke);
                list->AddLine(point(c, s, 5, 20), point(c, s, 12, 20), color, stroke);
                list->AddLine(point(c, s, 9, 12), point(c, s, 18, 12), color, stroke);
                arrowHead(list, point(c, s, 21.5f, 12), ImVec2(1, 0), s * 0.25f, color);
                break;

            case Icon::Computer:
                list->AddRect(point(c, s, 3, 5), point(c, s, 21, 16), color, s * 0.04f, 0, stroke);
                list->AddRectFilled(point(c, s, 1, 18), point(c, s, 23, 20), color);
                break;

            case Icon::Phone:
                list->AddRect(point(c, s, 7, 2), point(c, s, 17, 22), color, s * 0.08f, 0, stroke);
                list->AddRectFilled(point(c, s, 10.5f, 18.5f), point(c, s, 13.5f, 19.5f), color);
                break;
        }
    }
}
