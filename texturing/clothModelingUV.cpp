//
// Created by Media Team on 3/14/16.
//

#include "clothModelingUV.h"

namespace volcart {
    namespace texturing {

        // Constructor
        clothModelingUV::clothModelingUV( VC_MeshType::Pointer input,
                                          uint16_t unfurlIterations, uint16_t collideIterations, uint16_t expandIterations,
                                          cv::Vec3d plane_normal, PinIDs unfurlPins, PinIDs expansionPins ) :
                                            _mesh(input),
                                            _unfurlIterations(unfurlIterations), _unfurlPins(unfurlPins),
                                            _collideIterations(collideIterations),
                                            _expandIterations(expandIterations), _expansionPins(expansionPins)
        {

            // Create Dynamic world for bullet cloth simulation
            _WorldBroadphase = new btDbvtBroadphase();
            _WorldCollisionConfig = new btSoftBodyRigidBodyCollisionConfiguration();
            _WorldCollisionDispatcher = new btCollisionDispatcher(_WorldCollisionConfig);
            _WorldConstraintSolver = new btSequentialImpulseConstraintSolver();
            _WorldSoftBodySolver = new btDefaultSoftBodySolver();
            _World = new btSoftRigidDynamicsWorld( _WorldCollisionDispatcher,
                                                   _WorldBroadphase,
                                                   _WorldConstraintSolver,
                                                   _WorldCollisionConfig,
                                                   _WorldSoftBodySolver );

            // Add the collision plane at the origin
            btTransform startTransform( btQuaternion( 0, 0, 0, 1 ), btVector3( 0, 0, 0 ));
            btScalar mass = 0.f;
            btVector3 localInertia(0, 0, 0);
            btVector3 planeNormal( plane_normal(0), plane_normal(1), plane_normal(2) );
            btCollisionShape* groundShape = new btStaticPlaneShape( planeNormal, 0); // Normal + offset along normal
            btDefaultMotionState* myMotionState = new btDefaultMotionState(startTransform);
            btRigidBody::btRigidBodyConstructionInfo cInfo(mass, myMotionState, groundShape, localInertia);

            _collisionPlane = new btRigidBody(cInfo);
            _collisionPlane->setUserIndex(-1);
            _World->addRigidBody(_collisionPlane);

            // Softbody
            volcart::meshing::itk2bullet::itk2bullet( _mesh, _World->getWorldInfo(), &_softBody );

            // Scale the mesh so that max dimension <= 80m
            // Note: Assumes max is a positive coordinate. Not sure what this will do for small meshes
            btVector3 min, max;
            _softBody->getAabb( min, max );
            double maxDim = ( max.getX() < max.getY() ) ? max.getY() : max.getX();
            maxDim = ( maxDim < max.getZ() ) ? max.getZ() : maxDim;

            _meshToWorldScale = 80 / maxDim;
            _softBody->scale( btVector3( _meshToWorldScale, _meshToWorldScale, _meshToWorldScale) );
            _startingSurfaceArea = _SurfaceArea();

            // Set the mass for the whole cloth
            for ( int i = 0; i < _softBody->m_nodes.size(); ++i) {
                _softBody->setMass(i, 1);
            }

            // Add the softbody
            _softBody->randomizeConstraints();
            _softBody->updateNormals();
            _World->addSoftBody( _softBody );
        };

        // Destructor
        clothModelingUV::~clothModelingUV() {
            _World->removeSoftBody( _softBody );
            delete _softBody;

            _World->removeRigidBody( _collisionPlane );
            delete _collisionPlane->getMotionState();
            delete _collisionPlane;

            delete _World;
            delete _WorldConstraintSolver;
            delete _WorldSoftBodySolver;
            delete _WorldCollisionConfig;
            delete _WorldCollisionDispatcher;
            delete _WorldBroadphase;
        }

        // Process this mesh
        void clothModelingUV::run() {
            _unfurl();
            _collide();
            _expand();
        }

        // Get output
        VC_MeshType::Pointer clothModelingUV::getMesh() {
            VC_MeshType::Pointer output = VC_MeshType::New();
            volcart::meshing::deepCopy( _mesh, output );
            volcart::meshing::bullet2itk::bullet2itk( _softBody, output);
            return output;
        }

        ///// Simulation /////
        // Use gravity to unfurl the cloth
        void clothModelingUV::_unfurl()
        {
            // Set the simulation parameters
            _World->setInternalTickCallback( constrainMotionCallback, static_cast<void *>(this), true );
            _World->setGravity( btVector3(-10, 0, 0) );
            _softBody->getWorldInfo()->m_gravity = _World->getGravity();
            _softBody->m_cfg.kDP = 0.01; // Damping coefficient of the soft body [0,1]
            _softBody->m_materials[0]->m_kLST = 0.25; // Linear stiffness coefficient [0,1]
            _softBody->m_materials[0]->m_kAST = 0.25; // Area/Angular stiffness coefficient [0,1]
            _softBody->m_materials[0]->m_kVST = 0.25; // Volume stiffness coefficient [0,1]

            // Set the pins to not move
            for ( auto it = _unfurlPins.begin(); it != _unfurlPins.end(); ++it ) {
                _softBody->setMass( *it, 0.f );
            }

            // Run the simulation
            for ( uint16_t i = 0; i < _unfurlIterations; ++i ) {
                std::cerr << "volcart::texturing::clothUV: Unfurling " << i+1 << "/" << _unfurlIterations << std::flush;
                _World->stepSimulation(1 / 60.f, 10);
                _softBody->solveConstraints();
            }
            std::cerr << std::endl;
        }

        // Collide the cloth with the plane
        void clothModelingUV::_collide()
        {
            // Set the simulation parameters
            _World->setInternalTickCallback( axisLockCallback, static_cast<void *>(this), true );
            _World->setGravity(btVector3(0, -10, 0));
            _collisionPlane->setFriction(0); // (0-1] Default: 0.5
            _softBody->getWorldInfo()->m_gravity = _World->getGravity();
            _softBody->m_cfg.kDF = 0.1; // Dynamic friction coefficient (0-1] Default: 0.2
            _softBody->m_cfg.kDP = 0.01; // Damping coefficient of the soft body [0,1]

            // Reset all pins to move
            for ( auto n = 0; n < _softBody->m_nodes.size(); ++n ) {
                _softBody->setMass( n, 1 );
            }

            // Run the simulation
            for ( uint16_t i = 0; i < _collideIterations; ++i ) {
                std::cerr << "volcart::texturing::clothUV: Colliding " << i+1 << "/" << _collideIterations << std::flush;
                _World->stepSimulation(1 / 60.f, 10);
                _softBody->solveConstraints();
            }
            std::cerr << std::endl;
        }

        // Expand the edges of the cloth to iron out wrinkles, then let it relax
        void clothModelingUV::_expand()
        {
            // Set the simulation parameters
            _World->setInternalTickCallback( moveTowardTargetCallback, static_cast<void *>(this), true );
            _World->setGravity(btVector3(0, 0, 0));
            _collisionPlane->setFriction(0); // (0-1] Default: 0.5
            _softBody->getWorldInfo()->m_gravity = _World->getGravity();
            _softBody->m_cfg.kDF = 0.1; // Dynamic friction coefficient (0-1] Default: 0.2
            _softBody->m_cfg.kDP = 0.01; // Damping coefficient of the soft body [0,1]
            _softBody->m_materials[0]->m_kLST = 1.0; // Linear stiffness coefficient [0,1]
            _softBody->m_materials[0]->m_kAST = 1.0; // Area/Angular stiffness coefficient [0,1]
            _softBody->m_materials[0]->m_kVST = 1.0; // Volume stiffness coefficient [0,1]

            // Get the current XZ center of the softBody
            btVector3 min, max;
            _softBody->getAabb( min, max );
            btVector3 center;
            center.setX( (max.getX() + min.getX())/2 );
            center.setY( 0 );
            center.setZ( (max.getZ() + min.getZ())/2 );

            // Setup the expansion pins
            Pin newPin;
            _currentPins.clear();
            for ( auto it = _expansionPins.begin(); it != _expansionPins.end(); ++it ) {
                newPin.index = *it;
                newPin.node = &_softBody->m_nodes[*it];
                newPin.node->m_x.setY(0); // Force the pin to the Y-plane
                newPin.target = ( newPin.node->m_x - center ) * 1.5;

                _currentPins.push_back( newPin );
            }

            // Expand the edges
            for ( uint16_t i = 0; i < _expandIterations; ++i ) {
                std::cerr << "volcart::texturing::clothUV: Expanding " << i+1 << "/" << _expandIterations << std::flush;
                _World->stepSimulation(1 / 60.f, 10);
                _softBody->solveConstraints();
            }
            std::cerr << std::endl;

            // Relax the springs
            _World->setInternalTickCallback( emptyPreTickCallback, static_cast<void *>(this), true );
            _World->setGravity(btVector3(0, -20, 0));
            _softBody->m_cfg.kDP = 0.1;
            int counter = 0;
            double relativeError = std::fabs( (_startingSurfaceArea - _SurfaceArea()) / _startingSurfaceArea );
            while ( relativeError > 0.05 ) {
                std::cerr << "volcart::texturing::clothUV: Relaxing " << counter+1 << std::flush;
                _World->stepSimulation(1 / 60.f, 10);
                _softBody->solveConstraints();

                ++counter;
                if ( counter % 10 == 0 ) relativeError = std::fabs( (_startingSurfaceArea - _SurfaceArea()) / _startingSurfaceArea );
                if ( counter >= _expandIterations * 6 ) {
                    std::cerr << std::endl << "volcart::texturing::clothUV: Warning: Max iterations reached" << std::endl;
                    std::cerr << std::endl << "volcart::texturing::clothUV: Mesh Area Relative Error: " << relativeError << std::endl;
                    break;
                }
            }
            std::cerr << std::endl;
        }

        ///// Helper Functions /////

        // Calculate the surface area of the mesh using Heron's formula
        // Let a,b,c be the lengths of the sides of a triangle and p the semiperimeter
        // p = (a +  b + c) / 2
        // area of triangle = sqrt( p * (p - a) * (p - b) * (p - c) )
        double clothModelingUV::_SurfaceArea() {
            double surface_area = 0;
            for(int i = 0; i < _softBody->m_faces.size(); ++i) {
                double a = 0, b = 0, c = 0, p = 0;
                a = _softBody->m_faces[i].m_n[0]->m_x.distance(_softBody->m_faces[i].m_n[1]->m_x);
                b = _softBody->m_faces[i].m_n[0]->m_x.distance(_softBody->m_faces[i].m_n[2]->m_x);
                c = _softBody->m_faces[i].m_n[1]->m_x.distance(_softBody->m_faces[i].m_n[2]->m_x);

                p = (a + b + c) / 2;

                surface_area += sqrt( p * (p - a) * (p - b) * (p - c) );
            }

            return surface_area;
        }

        ///// Callback Functions /////
        void clothModelingUV::_constrainMotion( btScalar timeStep ) {
            for ( auto n = 0; n < _softBody->m_nodes.size(); ++n )
            {
                btVector3 velocity = _softBody->m_nodes[n].m_v;
                velocity.setY(0);
                velocity.setZ(0);
                _softBody->m_nodes[n].m_v = velocity;
            }
        };

        void clothModelingUV::_axisLock( btScalar timeStep ) {
            for ( auto n = 0; n < _softBody->m_nodes.size(); ++n )
            {
                btVector3 pos = _softBody->m_nodes[n].m_x;
                if ( pos.getY() < 0.0 ) {
                    btVector3 velocity = _softBody->m_nodes[n].m_v;
                    velocity.setY( velocity.getY() * -1 );
                    _softBody->m_nodes[n].m_v = velocity;
                }
            }
        };

        void clothModelingUV::_moveTowardTarget( btScalar timeStep ) {
            for ( auto pin = _currentPins.begin(); pin < _currentPins.end(); ++pin ) {
                btVector3 delta = ( pin->target - pin->node->m_x ).normalized();
                pin->node->m_v += delta/timeStep;
            }
        };

        void clothModelingUV::_emptyPreTick( btScalar timeStep ) {
            // This call back is used to disable other callbacks
        };

        //// Callbacks ////
        // Forward pretick callbacks to functions in a stupid Bullet physics way
        void constrainMotionCallback(btDynamicsWorld *world, btScalar timeStep) {
            clothModelingUV *w = static_cast<clothModelingUV *>( world->getWorldUserInfo() );
            w->_constrainMotion(timeStep);
        }

        void axisLockCallback(btDynamicsWorld *world, btScalar timeStep) {
            clothModelingUV *w = static_cast<clothModelingUV *>( world->getWorldUserInfo() );
            w->_axisLock(timeStep);
        }

        void moveTowardTargetCallback(btDynamicsWorld *world, btScalar timeStep) {
            clothModelingUV *w = static_cast<clothModelingUV *>( world->getWorldUserInfo() );
            w->_moveTowardTarget(timeStep);
        }

        void emptyPreTickCallback(btDynamicsWorld *world, btScalar timeStep) {
            clothModelingUV *w = static_cast<clothModelingUV *>( world->getWorldUserInfo() );
            w->_emptyPreTick(timeStep);
        }

    } // texturing
} // volcart