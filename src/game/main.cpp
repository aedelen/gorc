#include "framework/event/eventbus.h"
#include "framework/events/printevent.h"
#include "framework/events/exitingevent.h"
#include "framework/events/exitevent.h"
#include "framework/events/shutdownevent.h"
#include <iostream>
#include <cmath>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include "framework/diagnostics/streamreport.h"
#include "content/vfs/virtualfilesystem.h"
#include "content/manager.h"

#include "content/assets/level.h"

#include <GL/glu.h>
#include <set>
#include <btBulletDynamicsCommon.h>
#include <SFML/Audio.hpp>

sf::Sound AmbientSound;

class PhysicsDebugDraw : public btIDebugDraw {
	virtual void drawLine(const btVector3& from, const btVector3& to, const btVector3& color) override {
		glDisable(GL_TEXTURE_2D);

		glBegin(GL_LINES);

		glColor3fv(color.m_floats);
		glVertex3fv(from.m_floats);

		glColor3fv(color.m_floats);
		glVertex3fv(to.m_floats);

		glEnd();
		return;
	}

	virtual void drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color) override {
		return;
	}

	virtual void reportErrorWarning(const char* warningString) override {
		return;
	}

	virtual void draw3dText(const btVector3& location, const char* textString) override {
		return;
	}

	virtual void setDebugMode(int debugMode) override {
		return;
	}

	virtual int getDebugMode() const override {
		return DBG_DrawWireframe | DBG_DrawContactPoints;
	}
};

const double gameplayTick = (1.0 / 60.0);

using namespace Gorc;
using namespace Gorc::Math;

Vector<3> CameraPosition = Zero<3>();
Vector<3> CameraLook = Vec(0.0f, 1.0f, 0.0f);
Vector<3> CameraUp = Vec(0.0f, 0.0f, 1.0f);
float CameraRadius = 0.06f;
//size_t CameraCurrentSector = 104;
size_t CameraCurrentSector = 300;
Vector<3> CameraVelocity = Zero<3>();

void UpdateCameraAmbientSound(const Gorc::Content::Assets::Level& lev) {
	const auto& sec = lev.Sectors[CameraCurrentSector];
	if(&sec.AmbientSound->Buffer == AmbientSound.GetBuffer() && AmbientSound.GetStatus() != sf::Sound::Stopped) {
		AmbientSound.SetVolume(sec.AmbientSoundVolume * 100.0f);
	}
	else if (sec.AmbientSound != nullptr) {
		AmbientSound.SetBuffer(sec.AmbientSound->Buffer);
		AmbientSound.SetLoop(true);
		AmbientSound.SetVolume(sec.AmbientSoundVolume * 100.0f);
		AmbientSound.Play();
	}
	else {
		AmbientSound.Stop();
	}
}

void SetCameraCurrentSector(size_t sec_num, const Gorc::Content::Assets::Level& lev) {
	if(CameraCurrentSector != sec_num) {
		CameraCurrentSector = sec_num;
		UpdateCameraAmbientSound(lev);
	}
}

bool CameraInsideSector(const Gorc::Content::Assets::Level& lev, const Gorc::Content::Assets::LevelSector& sec) {
	for(size_t i =  sec.FirstSurface; i < sec.FirstSurface + sec.SurfaceCount; ++i) {
		const auto& surf = lev.Surfaces[i];
		const auto& p = lev.Vertices[std::get<0>(surf.Vertices[0])];

		if(Dot(surf.Normal, CameraPosition - p) < 0.0f) {
			return false;
		}
	}

	return true;
}

bool CameraPathPassesThroughAdjoin(const Vector<3>& oldCameraPosition, const Gorc::Content::Assets::Level& lev,
		const Gorc::Content::Assets::LevelSector& sec, const Gorc::Content::Assets::LevelSurface& surf) {
	if(Dot(CameraPosition - oldCameraPosition, surf.Normal) > 0.0f) {
		return false;
	}

	auto p = lev.Vertices[std::get<0>(surf.Vertices[0])];
	auto u = Dot(surf.Normal, p - oldCameraPosition) / Dot(surf.Normal, CameraPosition - oldCameraPosition);
	if(u < 0.0f || u > 1.0f) {
		return false;
	}

	return true;
}

void UpdateCameraSector(const Vector<3>& oldCameraPosition, const Gorc::Content::Assets::Level& lev) {
	std::vector<size_t> sectors { CameraCurrentSector };
	while(!sectors.empty()) {
		size_t sec_num = sectors.back();
		sectors.pop_back();

		const auto& sec = lev.Sectors[sec_num];

		if(CameraInsideSector(lev, sec)) {
			SetCameraCurrentSector(sec.Number, lev);
			return;
		}

		for(int i = sec.FirstSurface; i < sec.FirstSurface + sec.SurfaceCount; ++i) {
			const auto& surf = lev.Surfaces[i];
			if(surf.Adjoin >= 0 && !(surf.Flags & Gorc::Content::Assets::SurfaceFlag::Impassable)
					&& CameraPathPassesThroughAdjoin(oldCameraPosition, lev, sec, surf)) {
				sectors.push_back(surf.AdjoinedSector);
			}
		}
	}
}

void TranslateCamera(const Vector<3>& amt, const Gorc::Content::Assets::Level& lev) {
	CameraVelocity = Zero<3>();
	CameraVelocity += Get<X>(amt) * Cross(CameraLook, CameraUp);
	CameraVelocity += Get<Z>(amt) * CameraUp;
	CameraVelocity += Get<Y>(amt) * CameraLook;
}

void YawCamera(double amt) {
	float sint = std::sin(amt);
	float cost = std::cos(amt);

	Vector<3> NewLook = Vec(
			cost * Get<X>(CameraLook) - sint * Get<Y>(CameraLook),
			sint * Get<X>(CameraLook) + cost * Get<Y>(CameraLook),
			Get<Z>(CameraLook));
	Vector<3> NewUp = Vec(
			cost * Get<X>(CameraUp) - sint * Get<Y>(CameraUp),
			sint * Get<X>(CameraUp) + cost * Get<Y>(CameraUp),
			Get<Z>(CameraUp));

	CameraLook = Normalize(NewLook);
	CameraUp = Normalize(NewUp);
}

void PitchCamera(double amt) {
	float sint = std::sin(amt);
	float cost = std::cos(amt);

	Vector<3> NewUp = (-sint * CameraLook) + (cost * CameraUp);
	Vector<3> NewLook = (cost * CameraLook) + (sint * CameraUp);
	CameraUp = Normalize(NewUp);
	CameraLook = Normalize(NewLook);
}

void DrawLevel(const Gorc::Content::Assets::Level& level, double aspect) {
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glEnable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	Vector<3> CamLookPoint = CameraPosition + CameraLook;
	gluLookAt(Get<X>(CameraPosition), Get<Y>(CameraPosition), Get<Z>(CameraPosition),
			Get<X>(CamLookPoint), Get<Y>(CamLookPoint), Get<Z>(CamLookPoint),
			Get<X>(CameraUp), Get<Y>(CameraUp), Get<Z>(CameraUp));

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(70.0, aspect, 0.001, 1000.0);

	for(const auto& sector : level.Sectors) {
		for(size_t i = sector.FirstSurface; i < sector.SurfaceCount + sector.FirstSurface; ++i) {
			const auto& surface = level.Surfaces[i];
			if(surface.Material >= 0) {

				const auto& material_entry = level.MaterialEntries[surface.Material];
				const auto& material = level.Materials[surface.Material];

				float alpha = (surface.FaceTypeFlags & Content::Assets::FaceTypeFlag::Translucent) ? 0.5f : 1.0f;

				Vector<2> tex_scale = Vec(1.0f / static_cast<float>(material->Width),
						1.0f / static_cast<float>(material->Height));

				material->Cels[0].Diffuse->Bind();

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);

				glBegin(GL_TRIANGLES);

				Vector<3> first_geo = level.Vertices[std::get<0>(surface.Vertices[0])];
				Vector<2> first_tex = level.TextureVertices[std::get<1>(surface.Vertices[0])];
				float first_intensity = std::get<2>(surface.Vertices[0]) + sector.ExtraLight + surface.ExtraLight;

				for(size_t i = 2; i < surface.Vertices.size(); ++i) {
					Vector<3> second_geo = level.Vertices[std::get<0>(surface.Vertices[i - 1])];
					Vector<2> second_tex = level.TextureVertices[std::get<1>(surface.Vertices[i - 1])];
					float second_intensity = std::get<2>(surface.Vertices[i - 1]) + sector.ExtraLight + surface.ExtraLight;

					Vector<3> third_geo = level.Vertices[std::get<0>(surface.Vertices[i])];
					Vector<2> third_tex = level.TextureVertices[std::get<1>(surface.Vertices[i])];
					float third_intensity = std::get<2>(surface.Vertices[i]) + sector.ExtraLight + surface.ExtraLight;

					glTexCoord2f(Get<X>(first_tex) * Get<X>(tex_scale), Get<Y>(first_tex) * Get<Y>(tex_scale));
					glColor4f(first_intensity, first_intensity, first_intensity, alpha);
					glVertex3f(Get<X>(first_geo), Get<Y>(first_geo), Get<Z>(first_geo));

					glTexCoord2f(Get<X>(second_tex) * Get<X>(tex_scale), Get<Y>(second_tex) * Get<Y>(tex_scale));
					glColor4f(second_intensity, second_intensity, second_intensity, alpha);
					glVertex3f(Get<X>(second_geo), Get<Y>(second_geo), Get<Z>(second_geo));

					glTexCoord2f(Get<X>(third_tex) * Get<X>(tex_scale), Get<Y>(third_tex) * Get<Y>(tex_scale));
					glColor4f(third_intensity, third_intensity, third_intensity, alpha);
					glVertex3f(Get<X>(third_geo), Get<Y>(third_geo), Get<Z>(third_geo));
				}

				glEnd();

				/*if(sector.Number == CameraCurrentSector) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
					glDisable(GL_TEXTURE_2D);

					glBegin(GL_TRIANGLES);

					Vector<3> first_geo = level.Vertices[std::get<0>(surface.Vertices[0])];
					Vector<2> first_tex = level.TextureVertices[std::get<1>(surface.Vertices[0])];

					for(size_t i = 2; i < surface.Vertices.size(); ++i) {
						Vector<3> second_geo = level.Vertices[std::get<0>(surface.Vertices[i - 1])];
						Vector<2> second_tex = level.TextureVertices[std::get<1>(surface.Vertices[i - 1])];

						Vector<3> third_geo = level.Vertices[std::get<0>(surface.Vertices[i])];
						Vector<2> third_tex = level.TextureVertices[std::get<1>(surface.Vertices[i])];

						glTexCoord2f(Get<X>(first_tex) * Get<X>(tex_scale), Get<Y>(first_tex) * Get<Y>(tex_scale));
						glColor4f(1.0f, 1.0f, 0.6f, 1.0f);
						glVertex3f(Get<X>(first_geo), Get<Y>(first_geo), Get<Z>(first_geo));

						glTexCoord2f(Get<X>(second_tex) * Get<X>(tex_scale), Get<Y>(second_tex) * Get<Y>(tex_scale));
						glColor4f(1.0f, 1.0f, 0.6f, 1.0f);
						glVertex3f(Get<X>(second_geo), Get<Y>(second_geo), Get<Z>(second_geo));

						glTexCoord2f(Get<X>(third_tex) * Get<X>(tex_scale), Get<Y>(third_tex) * Get<Y>(tex_scale));
						glColor4f(1.0f, 1.0f, 0.6f, 1.0f);
						glVertex3f(Get<X>(third_geo), Get<Y>(third_geo), Get<Z>(third_geo));
					}

					glEnd();

					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					glEnable(GL_TEXTURE_2D);
				}*/
			}
		}
	}
}

class CameraObjectData;
class SurfaceObjectData;

class PhysicsObjectDataVisitor {
public:
	virtual ~PhysicsObjectDataVisitor() {
		return;
	}

	virtual void VisitCameraObjectData(const CameraObjectData&) = 0;
	virtual void VisitSurfaceObjectData(const SurfaceObjectData&) = 0;
};

class PhysicsObjectData {
public:
	virtual ~PhysicsObjectData() {
		return;
	}
	virtual void Accept(PhysicsObjectDataVisitor&) const = 0;
};

class CameraObjectData : public PhysicsObjectData {
public:
	virtual void Accept(PhysicsObjectDataVisitor& v) const override {
		v.VisitCameraObjectData(*this);
	}
};

class SurfaceObjectData : public PhysicsObjectData {
public:
	size_t SurfaceId;
	size_t SectorId;

	virtual void Accept(PhysicsObjectDataVisitor& v) const override {
		v.VisitSurfaceObjectData(*this);
	}
};

class SectorBroadphaseFilterCallback : public btOverlapFilterCallback {
private:
	mutable std::set<size_t> pcs;
	const Gorc::Content::Assets::Level& lev;

	void CameraBroadphaseVisitSector(const Gorc::Content::Assets::Level& lev, const Gorc::Content::Assets::LevelSector& sec, float max_dist) const {
		for(size_t i = sec.FirstSurface; i < sec.FirstSurface + sec.SurfaceCount; ++i) {
			const auto& surf = lev.Surfaces[i];

			if(surf.Adjoin >= 0 && !(surf.Flags & Gorc::Content::Assets::SurfaceFlag::Impassable)) {
				if(pcs.find(surf.AdjoinedSector) == pcs.end()) {
					float dist = Dot(CameraPosition - lev.Vertices[std::get<0>(surf.Vertices[0])], surf.Normal);
					if(dist >= 0.0f && dist <= max_dist) {
						pcs.insert(surf.AdjoinedSector);
						CameraBroadphaseVisitSector(lev, lev.Sectors[surf.AdjoinedSector], max_dist);
					}
				}
			}
		}
	}

	void CameraBroadphase(float max_dist) const {
		pcs.clear();
		pcs.insert(CameraCurrentSector);
		CameraBroadphaseVisitSector(lev, lev.Sectors[CameraCurrentSector], max_dist);
	}

public:
	SectorBroadphaseFilterCallback(const Gorc::Content::Assets::Level& lev) : lev(lev) {
		return;
	}

	virtual bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override {
		bool maskCollides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		maskCollides = maskCollides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;
		if(!maskCollides) {
			return false;
		}

		btCollisionObject* object0 = reinterpret_cast<btCollisionObject*>(proxy0->m_clientObject);
		btCollisionObject* object1 = reinterpret_cast<btCollisionObject*>(proxy1->m_clientObject);

		PhysicsObjectData* userData0 = reinterpret_cast<PhysicsObjectData*>(object0->getUserPointer());
		PhysicsObjectData* userData1 = reinterpret_cast<PhysicsObjectData*>(object1->getUserPointer());

		CameraObjectData* camera = dynamic_cast<CameraObjectData*>(userData0);
		SurfaceObjectData* surface = dynamic_cast<SurfaceObjectData*>(userData1);

		if(!camera) {
			camera = dynamic_cast<CameraObjectData*>(userData1);
			surface = dynamic_cast<SurfaceObjectData*>(userData0);
		}

		if(!camera || !surface) {
			// Two surfaces colliding.
			return false;
		}
		else {
			CameraBroadphase((proxy0->m_aabbMax - proxy0->m_aabbMin).length() + (proxy1->m_aabbMax - proxy1->m_aabbMin).length());
			return pcs.count(surface->SectorId) > 0;
		}
	}
};

int main(int argc, char** argv) {
	sf::Window Window(sf::VideoMode(1280, 720, 32), "Gorc");
	Gorc::Event::EventBus eventBus;
	bool running = true;

	// Register core event handlers
	eventBus.AddHandler<Gorc::Events::PrintEvent>([](Gorc::Events::PrintEvent& e) {
		std::cout << e.message << std::endl;
	});

	eventBus.AddHandler<Gorc::Events::ExitEvent>([&eventBus, &running](Gorc::Events::ExitEvent& e) {
		// A component has requested an application exit.
		// Check if components can quit.
		Gorc::Events::ExitingEvent exitingEvent;
		eventBus.FireEvent(exitingEvent);

		if(exitingEvent.Cancel) {
			return;
		}

		// All components can quit. Request shutdown.
		Gorc::Events::ShutdownEvent shutdownEvent;
		eventBus.FireEvent(shutdownEvent);

		running = false;
	});

	// HACK: Load test assets
	Gorc::Diagnostics::StreamReport Report(std::cout);
	Gorc::Content::VFS::VirtualFileSystem vfs("game/restricted",
			"game/resource", "game/episode", Report);
	Gorc::Content::Manager manager(Report, vfs);

	// Temporary: load 01narshadda.jkl for display.
	for(size_t i = 0; i < vfs.GetEpisodeCount(); ++i) {
		if(boost::iequals(vfs.GetEpisode(i).GetEpisodeName(), "The Force Within")) {
			vfs.SetEpisode(i);
		}
	}

	const auto& lev = manager.Load<Gorc::Content::Assets::Level>("04escapehouse.jkl");

	// Set camera position to centroid of current sector.
	CameraPosition = lev.Sectors[CameraCurrentSector].Center;

	const sf::Input& input = Window.GetInput();

	// Initialize Bullet
	SectorBroadphaseFilterCallback filterCallback(lev);
	PhysicsDebugDraw debugDraw;
	std::unique_ptr<btBroadphaseInterface> broadphase(new btDbvtBroadphase());
	std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration(new btDefaultCollisionConfiguration());
	std::unique_ptr<btCollisionDispatcher> dispatcher(new btCollisionDispatcher(collisionConfiguration.get()));
	std::unique_ptr<btSequentialImpulseConstraintSolver> solver(new btSequentialImpulseConstraintSolver);
	std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld(new btDiscreteDynamicsWorld(dispatcher.get(), broadphase.get(), solver.get(), collisionConfiguration.get()));
	dynamicsWorld->setDebugDrawer(&debugDraw);
	dynamicsWorld->setGravity(btVector3(0, 0, -lev.Header.WorldGravity));
	dynamicsWorld->getPairCache()->setOverlapFilterCallback(&filterCallback);

	// Create level trimeshes.
	std::vector<int> index_buffer;
	for(const auto& surf : lev.Surfaces) {
		const auto& first_vx = std::get<0>(surf.Vertices[0]);
		for(int j = 2; j < surf.Vertices.size(); ++j) {
			index_buffer.push_back(first_vx);
			index_buffer.push_back(std::get<0>(surf.Vertices[j - 1]));
			index_buffer.push_back(std::get<0>(surf.Vertices[j]));
		}
	}

	std::vector<float> vertex_buffer;
	for(const auto& vert : lev.Vertices) {
		vertex_buffer.push_back(Get<X>(vert));
		vertex_buffer.push_back(Get<Y>(vert));
		vertex_buffer.push_back(Get<Z>(vert));
	}

	std::vector<std::unique_ptr<btTriangleIndexVertexArray>> index_vertex_arrays;
	std::vector<std::unique_ptr<btBvhTriangleMeshShape>> surface_shapes;
	std::vector<std::unique_ptr<btRigidBody>> surface_rigid_bodies;
	std::unique_ptr<btDefaultMotionState> level_motion_state(new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1), btVector3(0,0,0))));
	std::vector<SurfaceObjectData> surface_object_data(lev.Surfaces.size());

	unsigned int CollideTypeWall = 1;
	unsigned int CollideTypeAdjoin = 2;
	unsigned int CollideTypePlayer = 4;

	size_t i = 0;
	for(const auto& surf : lev.Surfaces) {
		index_vertex_arrays.emplace_back(new btTriangleIndexVertexArray(
			surf.Vertices.size() - 2, &index_buffer[i], sizeof(int) * 3, vertex_buffer.size() / 3, &vertex_buffer[0], sizeof(float) * 3));
		surface_shapes.emplace_back(new btBvhTriangleMeshShape(index_vertex_arrays.back().get(), true));

		surface_rigid_bodies.emplace_back(new btRigidBody(
				btRigidBody::btRigidBodyConstructionInfo(0, level_motion_state.get(), surface_shapes.back().get(), btVector3(0,0,0))));

		unsigned int CollideType = 0;
		if(surf.Adjoin >= 0) {
			CollideType = CollideTypeAdjoin;
		}
		else {
			CollideType = CollideTypeWall;
		}

		unsigned int CollidesWith = 0;
		if(surf.Adjoin < 0 || (surf.Flags & Gorc::Content::Assets::SurfaceFlag::Impassable) ||
				!(lev.Adjoins[surf.Adjoin].Flags & Gorc::Content::Assets::SurfaceAdjoinFlag::AllowMovement) ||
				(lev.Adjoins[surf.Adjoin].Flags & Gorc::Content::Assets::SurfaceAdjoinFlag::AllowAiOnly)) {
			CollidesWith = CollideTypePlayer;
		}

		dynamicsWorld->addRigidBody(surface_rigid_bodies.back().get(), CollideType, CollidesWith);

		i += (surf.Vertices.size() - 2) * 3;
	}

	for(const auto& sec : lev.Sectors) {
		for(size_t i = sec.FirstSurface; i < sec.FirstSurface + sec.SurfaceCount; ++i) {
			surface_object_data[i].SurfaceId = i;
			surface_object_data[i].SectorId = sec.Number;
		}
	}

	for(size_t i = 0; i < lev.Surfaces.size(); ++i) {
		surface_rigid_bodies[i]->setUserPointer(&surface_object_data[i]);
	}

	std::unique_ptr<btCollisionShape> camera_shape(new btSphereShape(CameraRadius));

	std::unique_ptr<btDefaultMotionState> camera_motion_state(new btDefaultMotionState(btTransform(btQuaternion(0,0,0,1),
			btVector3(Get<X>(CameraPosition), Get<Y>(CameraPosition), Get<Z>(CameraPosition)))));
	btScalar mass = 1;
	btVector3 fallInertia(0,0,0);
	camera_shape->calculateLocalInertia(mass, fallInertia);
	std::unique_ptr<btRigidBody> camera_rigid_body(new btRigidBody(
			btRigidBody::btRigidBodyConstructionInfo(mass, camera_motion_state.get(), camera_shape.get(), fallInertia)));
	camera_rigid_body->setFlags(BT_DISABLE_WORLD_GRAVITY);
	camera_rigid_body->setSleepingThresholds(0, 0);
	CameraObjectData camera_object_data;
	camera_rigid_body->setUserPointer(&camera_object_data);

	dynamicsWorld->addRigidBody(camera_rigid_body.get(), CollideTypePlayer, CollideTypeWall | CollideTypeAdjoin);

	UpdateCameraAmbientSound(lev);
	// END HACK

	// Game loop:
	sf::Clock clock;
	double gameplayAccumulator = 0.0;
	double gameplayElapsedTime = 0.0;

	sf::Event event;
	while(running) {
		while(Window.GetEvent(event)) {
			if(event.Type == sf::Event::Closed) {
				Gorc::Events::ExitEvent exitEvent;
				eventBus.FireEvent(exitEvent);
			}
			else if(event.Type == sf::Event::Resized) {
				glViewport(0, 0, event.Size.Width, event.Size.Height);
			}
		}

		Window.SetActive();

		double currentTime = clock.GetElapsedTime();
		clock.Reset();

		gameplayElapsedTime += currentTime;
		gameplayAccumulator += currentTime;
		if(gameplayAccumulator >= gameplayTick) {
			gameplayAccumulator -= gameplayTick;

			// TODO: Update simulation here
			CameraVelocity = Zero<3>();

			if(input.IsKeyDown(sf::Key::Escape)) {
				Gorc::Events::ExitEvent evt;
				eventBus.FireEvent(evt);
			}

			Vector<3> Translate = Zero<3>();
			if(input.IsKeyDown(sf::Key::W)) {
				Translate += Vec(0.0f, 1.0f, 0.0f);
			}

			if(input.IsKeyDown(sf::Key::A)) {
				Translate += Vec(-1.0f, 0.0f, 0.0f);
			}

			if(input.IsKeyDown(sf::Key::S)) {
				Translate += Vec(0.0f, -1.0f, 0.0f);
			}

			if(input.IsKeyDown(sf::Key::D)) {
				Translate += Vec(1.0f, 0.0f, 0.0f);
			}

			if(input.IsKeyDown(sf::Key::Q)) {
				YawCamera(gameplayTick * 3.0f);
			}

			if(input.IsKeyDown(sf::Key::E)) {
				YawCamera(-gameplayTick * 3.0f);
			}

			if(input.IsKeyDown(sf::Key::PageUp)) {
				PitchCamera(-gameplayTick * 1.5f);
			}

			if(input.IsKeyDown(sf::Key::PageDown)) {
				PitchCamera(gameplayTick * 1.5f);
			}

			if(Length(Translate) > std::numeric_limits<float>::epsilon()) {
				Translate = Normalize(Translate);

				Translate *= gameplayTick;
				TranslateCamera(Translate, lev);
			}

			CameraVelocity *= 60.0f;

			camera_rigid_body->setLinearVelocity(btVector3(Get<X>(CameraVelocity), Get<Y>(CameraVelocity), Get<Z>(CameraVelocity)));

			dynamicsWorld->stepSimulation(gameplayTick, 10);

			auto OldCameraPosition = CameraPosition;
			btTransform trans;
			camera_rigid_body->getMotionState()->getWorldTransform(trans);
			btVector3 cam_origin = trans.getOrigin();

			CameraPosition = Vec(cam_origin.getX(), cam_origin.getY(), cam_origin.getZ());
			UpdateCameraSector(OldCameraPosition, lev);
		}

		// TODO: Render here
		glClearDepth(1.0f);
		glClearColor(0.392f, 0.584f, 0.929f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		DrawLevel(lev, static_cast<double>(Window.GetWidth()) / static_cast<double>(Window.GetHeight()));
		//dynamicsWorld->debugDrawWorld();

		Window.Display();
	}

	return 0;
}
