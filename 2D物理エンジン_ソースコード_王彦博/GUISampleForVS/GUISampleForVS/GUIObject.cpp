// ================================================================================
// GUIObject
// 編集するファイル
// ================================================================================

#include "stdafx.h"
#include "GUIObject.h"
#include <time.h>
#include <vector>
#include <algorithm>
#include <limits> // std::numeric_limits を使用するため

/*
【物理エンジン最終版：分離軸定理(SAT)実装バージョン】
三角形の衝突問題を根本的に解決するため、衝突検知アルゴリズムをAABB近似から、
より正確な「分離軸定理（Separating Axis Theorem, SAT）」に全面的に置き換えました。

SATとは？
二つの凸多角形について、「ある直線上に二つの図形を射影（影を落とす）したとき、その影が重ならないような直線」
が一本でも見つかれば、二つの図形は衝突していないと判断できます。
逆に、考えられるすべての軸で影が重なっていれば、それらは衝突していると結論付けられます。
このアルゴリズムにより、回転した三角形や四角形同士の衝突をピクセルレベルで正確に検出できるようになります。
*/



// コップ（外枠）の形を定義
const FXY g_sCupPoint[4] = { { 100.0f, 100.0f },{ 100.0f, 600.0f },{ 600.0f, 600.0f },{ 600.0f, 100.0f } };

// 図形の種類
enum class ShapeType {
	CIRCLE,    // 円
	RECTANGLE, // 四角形
	TRIANGLE   // 三角形
};

// 物理オブジェクトのデータ構造
struct Object {
	ShapeType type;       // 形の種類
	FXY position;         // 現在の座標
	FXY velocity;         // 動くスピードと方向
	float angle;          // 回転角度
	UINT color;           // 図形の色
	float mass;           // 重さ（面積から計算）
	float inv_mass;       // 重さの逆数（計算を速くするために保持）
	float restitution;    // 跳ね返り係数（0〜1）
	float radius;         // 円の場合の半径
	float width, height;  // 四角形・三角形の幅と高さ
	FXY local_vertices[4];// 図形自体の角の座標（回転前の中心からの位置）
	int vertex_count;     // 頂点の数
};

std::vector<Object> g_objects; // 画面上の全オブジェクトを管理するリスト

// 物理シミュレーションの設定値
namespace PhysicsConstants {
	const float GRAVITY = 0.2f;                   // 重力の強さ
	const int SUB_STEPS = 10;                     // 1フレーム内の計算回数（多いほど正確）
	const float DAMPING = 0.995f;                 // 空気抵抗（少しずつ減速）
	const float POSITIONAL_CORRECTION_PERCENT = 0.3f; // めり込み補正の強さ
	const float SLOP = 0.05f;                     // 許容するわずかなめり込み
}

// 値を範囲内に収める関数
float Clamp(float value, float min, float max) {
	return std::max(min, std::min(value, max));
}

// 乱数の種を設定
UINT GetRandomSeed() {
	return static_cast<UINT>(time(NULL));
}

// フレーム更新の間隔（ミリ秒）
UINT GetUpdateMsec() {
	return 16; // 約60fps
}

// --- SATアルゴリズム用の補助関数 ---

// 図形の「現在の向きと位置」に合わせた頂点の座標を計算する
void GetWorldVertices(const Object& obj, FXY* vertices) {
	for (int i = 0; i < obj.vertex_count; ++i) {
		FXY rotated_v;
		FXY temp_v = obj.local_vertices[i];
		// 中心を基準に回転させる
		GetRotPos(&rotated_v, { 0,0 }, &temp_v, 1, obj.angle);
		// 回転した座標に、現在の中心位置を足す（ワールド座標へ変換）
		vertices[i] = rotated_v + obj.position;
	}
}

// 図形を特定の軸（直線）に投影して、影の「最小値」と「最大値」を求める
void ProjectPolygon(const FXY& axis, const FXY* vertices, int vertex_count, float& min_proj, float& max_proj) {
	min_proj = Vector2DDot(vertices[0], axis);
	max_proj = min_proj;
	for (int i = 1; i < vertex_count; ++i) {
		float p = Vector2DDot(vertices[i], axis);
		if (p < min_proj) min_proj = p;
		else if (p > max_proj) max_proj = p;
	}
}

// 【多角形同士の衝突検知】
// 戻り値: 衝突していればtrue。normalに押し出す方向、overlapに重なり具合を返す。
bool PolygonCollision(Object& obj1, Object& obj2, FXY& normal, float& overlap) {
	FXY vertices1[4], vertices2[4];
	GetWorldVertices(obj1, vertices1);
	GetWorldVertices(obj2, vertices2);

	overlap = std::numeric_limits<float>::max();

	// 2つの図形のすべての辺について、その垂直な方向（法線）をテストする
	for (int shape_idx = 0; shape_idx < 2; ++shape_idx) {
		const FXY* current_vertices = (shape_idx == 0) ? vertices1 : vertices2;
		int current_vertex_count = (shape_idx == 0) ? obj1.vertex_count : obj2.vertex_count;

		for (int i = 0; i < current_vertex_count; ++i) {
			FXY p1 = current_vertices[i];
			FXY p2 = current_vertices[(i + 1) % current_vertex_count];
			FXY edge = p2 - p1;
			FXY axis = { -edge.y, edge.x }; // 辺に対して垂直な軸を作る
			VectorNormalize(axis);

			float min1, max1, min2, max2;
			ProjectPolygon(axis, vertices1, obj1.vertex_count, min1, max1);
			ProjectPolygon(axis, vertices2, obj2.vertex_count, min2, max2);

			// 影が重なっていない軸が一つでもあれば、それは衝突していない（分離軸）
			if (max1 < min2 || max2 < min1) {
				return false;
			}

			// 最も重なりが小さい軸を探す（これが最小移動ベクトル：MTVになる）
			float current_overlap = std::min(max1, max2) - std::max(min1, min2);
			if (current_overlap < overlap) {
				overlap = current_overlap;
				normal = axis;
			}
		}
	}

	// 法線の向きが常にobj1からobj2を指すように調整
	FXY center_vec = obj2.position - obj1.position;
	if (Vector2DDot(center_vec, normal) < 0) {
		normal = -normal;
	}
	return true;
}

// 【円と多角形の衝突検知】
bool CirclePolygonCollision(Object& circle_obj, Object& poly_obj, FXY& normal, float& overlap) {
	FXY poly_vertices[4];
	GetWorldVertices(poly_obj, poly_vertices);

	overlap = std::numeric_limits<float>::max();

	// 1. ポリゴンの辺の法線方向をテスト
	for (int i = 0; i < poly_obj.vertex_count; i++) {
		FXY p1 = poly_vertices[i];
		FXY p2 = poly_vertices[(i + 1) % poly_obj.vertex_count];
		FXY edge = p2 - p1;
		FXY axis = { -edge.y, edge.x };
		VectorNormalize(axis);

		float min_poly, max_poly;
		ProjectPolygon(axis, poly_vertices, poly_obj.vertex_count, min_poly, max_poly);

		float circle_proj = Vector2DDot(circle_obj.position, axis);
		float min_circle = circle_proj - circle_obj.radius;
		float max_circle = circle_proj + circle_obj.radius;

		if (max_circle < min_poly || max_poly < min_circle) return false;

		float current_overlap = std::min(max_circle, max_poly) - std::max(min_circle, min_poly);
		if (current_overlap < overlap) {
			overlap = current_overlap;
			normal = axis;
		}
	}

	// 2. 円に最も近い頂点方向をテスト（角にぶつかる場合への対応）
	float closest_dist_sq = std::numeric_limits<float>::max();
	FXY closest_vertex;
	for (int i = 0; i < poly_obj.vertex_count; i++) {
		float d_x = poly_vertices[i].x - circle_obj.position.x;
		float d_y = poly_vertices[i].y - circle_obj.position.y;
		float dist_sq = d_x * d_x + d_y * d_y;
		if (dist_sq < closest_dist_sq) {
			closest_dist_sq = dist_sq;
			closest_vertex = poly_vertices[i];
		}
	}

	FXY axis = closest_vertex - circle_obj.position;
	VectorNormalize(axis);

	float min_poly, max_poly;
	ProjectPolygon(axis, poly_vertices, poly_obj.vertex_count, min_poly, max_poly);
	float circle_proj = Vector2DDot(circle_obj.position, axis);
	float min_circle = circle_proj - circle_obj.radius;
	float max_circle = circle_proj + circle_obj.radius;

	if (max_circle < min_poly || max_poly < min_circle) return false;

	float current_overlap = std::min(max_circle, max_poly) - std::max(min_circle, min_poly);
	if (current_overlap < overlap) {
		overlap = current_overlap;
		normal = axis;
	}

	FXY center_vec = poly_obj.position - circle_obj.position;
	if (Vector2DDot(center_vec, normal) < 0) normal = -normal;

	return true;
}

// --- メインロジック ---

// オブジェクトの初期化（リセット）
void ResetObject()
{
	TRACE(_T("Reset Object\n"));
	g_objects.clear();

	int totalObjects = RandomRange(10, 15); // 生成する個数

	for (int i = 0; i < totalObjects; ++i) {
		Object obj = {};
		obj.velocity = { 0.0f, 0.0f };
		obj.restitution = 0.2f;
		obj.type = static_cast<ShapeType>(Random(2)); // 0:円, 1:四角, 2:三角

		switch (obj.type) {
		case ShapeType::CIRCLE:
			obj.vertex_count = 0;
			obj.radius = RandomRangeF(15.0f, 35.0f);
			obj.color = RGB(144, 238, 144); // 緑
			obj.mass = F_PI * obj.radius * obj.radius;
			break;
		case ShapeType::RECTANGLE:
			obj.vertex_count = 4;
			obj.width = RandomRangeF(30.0f, 70.0f);
			obj.height = RandomRangeF(30.0f, 70.0f);
			obj.angle = RandomRangeF(0.0f, 360.0f);
			obj.color = RGB(30, 144, 255); // 青
			obj.mass = obj.width * obj.height;
			obj.local_vertices[0] = { -obj.width / 2.0f, -obj.height / 2.0f };
			obj.local_vertices[1] = { obj.width / 2.0f, -obj.height / 2.0f };
			obj.local_vertices[2] = { obj.width / 2.0f,  obj.height / 2.0f };
			obj.local_vertices[3] = { -obj.width / 2.0f,  obj.height / 2.0f };
			break;
		case ShapeType::TRIANGLE:
			obj.vertex_count = 3;
			float w = RandomRangeF(40.0f, 90.0f);
			float h = RandomRangeF(40.0f, 90.0f);
			obj.angle = RandomRangeF(0.0f, 360.0f);
			obj.color = RGB(250, 140, 0); // オレンジ
			obj.mass = 0.5f * w * h;
			obj.local_vertices[0] = { 0.0f, (2.0f * h) / 3.0f };
			obj.local_vertices[1] = { -w / 2.0f, -h / 3.0f };
			obj.local_vertices[2] = { w / 2.0f, -h / 3.0f };
			break;
		}

		obj.inv_mass = (obj.mass > 0.0f) ? 1.0f / obj.mass : 0.0f;

		// 重ならないように少しマージンを空けて配置
		float margin = 100.0f;
		obj.position.x = RandomRangeF(g_sCupPoint[0].x + margin, g_sCupPoint[3].x - margin);
		obj.position.y = RandomRangeF(g_sCupPoint[0].y + margin, g_sCupPoint[1].y - 250.0f);

		g_objects.push_back(obj);
	}
}

// 物理演算と描画の更新
void UpdateObject(HDC& hdc, UINT unTimer)
{
	// 1. 重力と空気抵抗の適用
	for (auto& obj : g_objects) {
		obj.velocity.y += PhysicsConstants::GRAVITY;
		obj.velocity *= PhysicsConstants::DAMPING;
		obj.position += obj.velocity;
	}

	// 2. 衝突判定（より正確にするためにサブステップ内で計算）
	for (int i = 0; i < PhysicsConstants::SUB_STEPS; ++i) {

		// カップ（壁）との衝突判定
		for (auto& obj : g_objects) {
			FXY aabb_min, aabb_max;
			// 境界ボックス(AABB)を計算
			if (obj.type == ShapeType::CIRCLE) {
				aabb_min = { obj.position.x - obj.radius, obj.position.y - obj.radius };
				aabb_max = { obj.position.x + obj.radius, obj.position.y + obj.radius };
			}
			else {
				FXY vertices[4];
				GetWorldVertices(obj, vertices);
				aabb_min = aabb_max = vertices[0];
				for (int v = 1; v < obj.vertex_count; ++v) {
					aabb_min.x = std::min(aabb_min.x, vertices[v].x);
					aabb_min.y = std::min(aabb_min.y, vertices[v].y);
					aabb_max.x = std::max(aabb_max.x, vertices[v].x);
					aabb_max.y = std::max(aabb_max.y, vertices[v].y);
				}
			}

			// 壁からはみ出したら押し戻す
			if (aabb_min.x < g_sCupPoint[0].x) {
				obj.position.x += g_sCupPoint[0].x - aabb_min.x;
				if (obj.velocity.x < 0) obj.velocity.x *= -obj.restitution;
			}
			if (aabb_max.x > g_sCupPoint[3].x) {
				obj.position.x -= aabb_max.x - g_sCupPoint[3].x;
				if (obj.velocity.x > 0) obj.velocity.x *= -obj.restitution;
			}
			if (aabb_max.y > g_sCupPoint[1].y) {
				obj.position.y -= aabb_max.y - g_sCupPoint[1].y;
				if (obj.velocity.y > 0) obj.velocity.y *= -obj.restitution;
			}
		}

		// オブジェクト同士の衝突
		for (size_t j = 0; j < g_objects.size(); ++j) {
			for (size_t k = j + 1; k < g_objects.size(); ++k) {
				Object& obj1 = g_objects[j];
				Object& obj2 = g_objects[k];

				FXY normal = { 0,0 };
				float overlap = 0.0f;
				bool collided = false;

				bool is_obj1_circle = (obj1.type == ShapeType::CIRCLE);
				bool is_obj2_circle = (obj2.type == ShapeType::CIRCLE);

				// 形の組み合わせによって判定関数を使い分ける
				if (is_obj1_circle && is_obj2_circle) {
					FXY axis = obj2.position - obj1.position;
					float dist_sq = VectorLength(axis) * VectorLength(axis);
					float total_radius = obj1.radius + obj2.radius;
					if (dist_sq < total_radius * total_radius && dist_sq > 0.0001f) {
						float dist = sqrtf(dist_sq);
						normal = axis / dist;
						overlap = total_radius - dist;
						collided = true;
					}
				}
				else if (is_obj1_circle && !is_obj2_circle) {
					collided = CirclePolygonCollision(obj1, obj2, normal, overlap);
				}
				else if (!is_obj1_circle && is_obj2_circle) {
					collided = CirclePolygonCollision(obj2, obj1, normal, overlap);
					normal = -normal;
				}
				else {
					collided = PolygonCollision(obj1, obj2, normal, overlap);
				}

				// 衝突した後の反発処理
				if (collided) {
					FXY rv = obj2.velocity - obj1.velocity;
					float velAlongNormal = Vector2DDot(rv, normal);
					if (velAlongNormal > 0) continue; // 既に離れようとしているなら無視

					float e = std::min(obj1.restitution, obj2.restitution);
					float impulse_j = -(1.0f + e) * velAlongNormal;
					impulse_j /= (obj1.inv_mass + obj2.inv_mass);
					FXY impulse = normal * impulse_j;

					// 速度を変化させる
					obj1.velocity -= impulse * obj1.inv_mass;
					obj2.velocity += impulse * obj2.inv_mass;

					// めり込みを物理的に解消する
					const float correction_amount = std::max(overlap - PhysicsConstants::SLOP, 0.0f) / (obj1.inv_mass + obj2.inv_mass) * PhysicsConstants::POSITIONAL_CORRECTION_PERCENT;
					FXY correction = normal * correction_amount;
					obj1.position -= correction * obj1.inv_mass;
					obj2.position += correction * obj2.inv_mass;
				}
			}
		}
	}

	// 3. 描画
	for (const auto& obj : g_objects) {
		if (obj.type == ShapeType::CIRCLE) {
			RenderCircle(hdc, obj.position.x, obj.position.y, obj.radius, obj.color);
		}
		else {
			FXY vertices[4];
			GetWorldVertices(obj, vertices);
			if (obj.type == ShapeType::RECTANGLE) {
				RenderRectangle(hdc, vertices, obj.color);
			}
			else {
				RenderTriangle(hdc, vertices, obj.color);
			}
		}
	}
}


// 補助関数セクション
// ここには、計算や描画を楽にするための「便利ツール」がまとまっています。
#pragma region Helper Functions

// 乱数取得.
int Random(int nMax) { return rand() % (nMax + 1); }
int RandomRange(int nMin, int nMax) { if (nMin > nMax) std::swap(nMin, nMax); if (nMin == nMax) return nMin; return nMin + (rand() % (nMax - nMin + 1)); }

// コップの描画.
void RenderCup(HDC& hdc) { const int pointCount = sizeof(g_sCupPoint) / sizeof(g_sCupPoint[0]); HPEN hPen = CreatePen(PS_SOLID, 5, RGB(0, 0, 0)); HGDIOBJ hOldObj = SelectObject(hdc, hPen); for (int i = 1; i < pointCount; i++) { MoveToEx(hdc, (int)g_sCupPoint[i - 1].x, (int)g_sCupPoint[i - 1].y, NULL); LineTo(hdc, (int)g_sCupPoint[i].x, (int)g_sCupPoint[i].y); } SelectObject(hdc, hOldObj); DeleteObject(hPen); }
// ２地点間の距離.
float CalcDistance(FXY sLhs, FXY sRhs) { FXY sSub = sRhs - sLhs; return sqrtf(sSub.x * sSub.x + sSub.y * sSub.y); }
// ベクトルのサイズ.
float VectorLength(FXY sXy) { return sqrtf(sXy.x * sXy.x + sXy.y * sXy.y); }
// ベクトル正規化.
void VectorNormalize(FXY& sXy) { float fLength = VectorLength(sXy); if (fLength > 0.0001f) { sXy.x /= fLength; sXy.y /= fLength; } }
// ベクトルの内積.
float Vector2DDot(FXY sLhs, FXY sRhs) { return (sLhs.x * sRhs.x) + (sLhs.y * sRhs.y); }
// 回転後の座標取得.
void GetRotPos(FXY* pOutput, FXY center, FXY* pPos, int nNum, float fAngle) { fAngle = FIT_ANGLE(fAngle); float rad = ANGLE(fAngle); float cosValue = cosf(rad); float sinValue = sinf(rad); for (int i = 0; i < nNum; i++) { FXY sBuf = pPos[i]; pOutput[i].x = center.x + (sBuf.x - center.x) * cosValue - (sBuf.y - center.y) * sinValue; pOutput[i].y = center.y + (sBuf.x - center.x) * sinValue + (sBuf.y - center.y) * cosValue; } }
// 多角形の描画.
void RenderPolygon(HDC hdc, POINT* pPos, int nNum, UINT rgb) { HBRUSH hBrush = CreateSolidBrush(rgb); HGDIOBJ hOldObj = SelectObject(hdc, hBrush); SetPolyFillMode(hdc, WINDING); Polygon(hdc, pPos, nNum); SelectObject(hdc, hOldObj); DeleteObject(hBrush); }
// 三角形の描画.
void RenderTriangle(HDC hdc, FXY* pPos, UINT rgb) { POINT sXy[3]; for (int i = 0; i < 3; i++) { sXy[i] = { (LONG)pPos[i].x, (LONG)pPos[i].y }; } RenderPolygon(hdc, sXy, 3, rgb); }
// 四角形の描画.
void RenderRectangle(HDC hdc, FXY* pPos, UINT rgb) { POINT sXy[4]; for (int i = 0; i < 4; i++) { sXy[i] = { (LONG)pPos[i].x, (LONG)pPos[i].y }; } RenderPolygon(hdc, sXy, 4, rgb); }
// 円の描画.
void RenderCircle(HDC hdc, float x, float y, float r, UINT rgb) { int nX = (int)x; int nY = (int)y; int nR = (int)r; HBRUSH hBrush = CreateSolidBrush(rgb); HGDIOBJ hOldObj = SelectObject(hdc, hBrush); Ellipse(hdc, nX - nR, nY - nR, nX + nR, nY + nR); SelectObject(hdc, hOldObj); DeleteObject(hBrush); }

// (元のコードに含まれていたが、このバージョンでは未使用の関数)
float RandomF(float fMax) { return 0.0f; }
float RandomRangeF(float fMin, float fMax) { if (fMin > fMax) std::swap(fMin, fMax); if (fMin == fMax) return fMin; return fMin + static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * (fMax - fMin); }
FXY VectorSub(FXY sFrom, FXY sTo) { return sTo - sFrom; }
float Vector2DCross(FXY sLhs, FXY sRhs) { return (sLhs.x * sRhs.y) - (sLhs.y * sRhs.x); }
void RenderTriangle(HDC hdc, POINT* pPos, UINT rgb) {}
void RenderRectangle(HDC hdc, POINT* pPos, UINT rgb) {}
void RenderRotTriangle(HDC hdc, FXY& center, FXY* pPos, float fAngle, UINT rgb) {}
void RenderRotRectangle(HDC hdc, FXY& center, FXY* pPos, float fAngle, UINT rgb) {}

#pragma endregion