//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include <p3d/scenegraph/scenegraph.hpp>
#include <p3d/anim/drawablepose.hpp>
#include <p3d/anim/pose.hpp>
#include <constants/chunks.h>
#include <constants/chunkids.hpp>
#include <p3d/chunkfile.hpp>
#include <p3d/utility.hpp>
#include <p3d/matrixstack.hpp>
#include <p3d/camera.hpp>
#include <p3d/light.hpp>
#include <p3d/SHARFrontendMenuCel.h>
#include <p3d/sharrendercontext.hpp>

#include <string.h>

#ifdef RAD_ANDROID
// These renderer controls are implemented in the Android GLES backend.
bool IsCelShadingEnabled();
int GetSHARCelShadingMode();
bool IsSHARFrontendMenuCelPassActive();
bool IsSHARGagCharacterCelPassActive();
void SetSHARCharacterMaskModelPass(bool enabled);
void SetCelShadingObjectEnabled(bool enabled);
void SetCelShadingOutlinePass(bool enabled);
void SetSHAREnvironmentCelPassActive(bool enabled);
void SetSHAREnvironmentFoliageMeshPassActive(bool enabled);

static bool SHARFrontendMenuUIDInList(
    tDrawable* drawable,
    const unsigned long long* uids,
    unsigned int count)
{
    if (drawable == NULL)
        return false;

    const unsigned long long drawableUID =
        static_cast<unsigned long long>(drawable->GetUID());
    for (unsigned int i = 0; i < count; ++i)
    {
        if (drawableUID == uids[i])
            return true;
    }
    return false;
}

// Base camset solid FE_ meshes, the two explicitly approved translucent
// exceptions, and the requested solid non-FE assets.
bool IsSHARFrontendMenuOutlineDrawable(tDrawable* drawable)
{
    if (drawable == NULL || !IsSHARFrontendMenuCelPassActive())
        return false;

    static const unsigned long long outlineUIDs[] =
    {
    0x232AC4D8CCDB63D8ULL,
    0x232AC4D8CCDB63D9ULL,
    0x232AC4D8CCDB63D5ULL,
    0x232AC4D8CCDE6499ULL,
    0x232AC4D8CCDE6498ULL,
    0x232AC4D8CCDD645AULL,
    0x232AC4D8CCDD645BULL,
    0x232AC4D8CCDD6458ULL,
    0x232AC4D8CCDD6459ULL,
    0x232AC4D8CCDD645EULL,
    0x232AC4D8CCDD645FULL,
    0x232AC4D8CCD8631DULL,
    0x232AC4D8CCD8631CULL,
    0x692E38261FA13B55ULL,
    0x8DAD8B78E565AB9FULL,
    0x8DAD8B78E565AB9EULL,
    0x8DAD8B78E565AB99ULL,
    0x8DAD8B78E565AB98ULL,
    0x8DAD8B78E565AB9BULL,
    0x8DAD8B78E565AB9AULL,
    0xE1F232902DF9B311ULL,
    0x2B6EBC07DA8360A6ULL,
    0x2B6EBC07DA8360A5ULL,
    0x2B6EBC07DA8360A3ULL,
    0x2B6EBC07DA8360A2ULL,
    0x2B6EBC07DA8360A1ULL,
    0x2B6EBC07DA8360A0ULL,
    0x2B6EBC07DA8360AFULL,
    0x2B6EBC07DA8360AEULL,
    0xC40AECBE792809ACULL,
    0xC40AECBE792809ADULL,
    0xC40AECBE792809AEULL,
    0x082B6CEE1A0FAD4AULL,
    0xD32023EB4BC504A7ULL,
    0x0A0416875202227FULL,
    0x3CEA6C496681F4A6ULL,
    0x211B469AAE9705E4ULL,
    0x211B469AAE9705E7ULL,
    0x69FA0C922EA134E8ULL,
    0x69FA0C922EA134E9ULL,
    0x69FA0C922EA134EEULL,
    0x69FA0C922EA134EFULL,
    0x69FA0C922EA134ECULL,
    0x69FA0C922EA134EDULL,
    0x69FA0C922EA134E2ULL,
    0x24A6FD48A6F42E04ULL,
    0x0151735656F5D451ULL,
    0xD809AE997F45B985ULL,
    0x56AAF926C24826D9ULL,
    0x2F5BBE0AF204A770ULL,
    0x2F5BBE0AF204A771ULL,
    0xA740069A1E1D88C3ULL,
    0xA740069A1E1D88C2ULL,
    0xA740069A1E1D88C5ULL,
    0xA740069A1E1D88C4ULL,
    0xA740069A1E1D88C7ULL,
    0xA740069A1E1D88C6ULL,
    0xA740069A1E1D88C9ULL,
    0xA740069A1E1D88C8ULL,
    0x5E8A6B0C619F7B39ULL,
    0x5E8A6B0C619F7B3AULL,
    0x5E8A6B0C619F7B3BULL,
    0x5E8A6B0C619F7B3CULL,
    0x5E8A6B0C619F7B3DULL,
    0x5E8A6B0C619F7B3EULL,
    0x5E8A6B0C619F7B3FULL,
    0x5E8A6B0C619F7B30ULL,
    0xE3AD130E6E7274FAULL,
    0xE3AD130E6E7274FBULL,
    0xE3AD130E6E7274FCULL,
    0xF4A2AFA3D9BBD93BULL,
    0xF4A2AFA3D9BBD93CULL,
    0xF4A2AFA3D9BBD93FULL,
    0xF4A2AFA3D9BBD93EULL,
    0xF4A2AFA3D9BBD931ULL,
    0xF4A2AFA3D9BBD930ULL,
    0x029A151C23E128E6ULL,
    0xB90755CDFD4F10AAULL,
    0xB90755CDFD4F10ABULL,
    0xB90755CDFD4F10A8ULL,
    0xB90755CDFD4F10A9ULL,
    0xB90755CDFD4F10AEULL,
    0xB90755CDFD4F10AFULL,
    0xB90755CDFD4F10ACULL,
    0x029A151C23E128E5ULL,
    0x029A151C23E128E4ULL,
    0x029A151C23E128E3ULL,
    0x029A151C23E128E2ULL,
    0x029A151C23E128E0ULL,
    0x029A151C23E128EFULL,
    0x029A151C23E128EEULL,
    0x645EDC24769A499FULL,
    0x1887D98DC6BCAE23ULL,
    0x1887D98DC6BDAE69ULL,
    0x58A2EA033B78B810ULL,
    0xBA1AD2445AC54BC1ULL,
    0xBA1AD2445AC54BC5ULL,
    0xBA1AD2445AC54BC6ULL,
    0x58A2EA033B78B813ULL,
    0x4EB6605080A405C5ULL,
    0xBF323473AE226B4AULL,
    0xA0F0AA1539C3022DULL,
    0x454F14FC392A8923ULL,
    0x454F14FC392A8922ULL,
    0x454F14FC392A8921ULL,
    0x454F14FC392A8920ULL,
    0x454F14FC392A8927ULL,
    0x454F14FC392A8926ULL,
    0x454F14FC392A8925ULL,
    0xA0F0AA1539C3022EULL,
    0xA0F0AA1539C3022FULL,
    0xA0F0AA1539C30228ULL,
    0xA0F0AA1539C30229ULL,
    0xA0F0AA1539C3022AULL,
    0xA0F0AA1539C3022BULL,
    0xA0F0AA1539C30224ULL,
    0xA0F0AA1539C30225ULL,
    0xA17BC4218D9E3526ULL,
    0xA17BC4218D9E3529ULL,
    0xA17BC4218D9833A7ULL,
    0xA17BC4218D9833A3ULL,
    0xA17BC4218D9833A2ULL,
    0x04CC74FA029DFB73ULL,
    0xC852195A3FA6ABE2ULL,
    0x84700E839B52DA46ULL,
    0x1141303E4693D998ULL,
    0xF525F3485B73D054ULL,
    0x56019487D391F74EULL,
    // Additional verified camset assets that were previously missing.
    0x95F914936F1B7374ULL, // tree1Shape (also classified as foliage)
    0xB1609118CB7D2F25ULL, // tree2Shape (also classified as foliage)
    0xD05AEC4B35673406ULL, // tree3Shape (also classified as foliage)
    0x4030F081FCE16156ULL, // pumpkinShape01
    0x56019487D391F74AULL, // pPlaneShape14 (curtain)
    0x56019487D391F74FULL, // pPlaneShape11 (curtain)
    0x3F67E8787EB4C083ULL, // polySurfaceShape1 (racetrack)
    0xF3FE519E30A7A345ULL, // polySurfaceShape137 (Thanksgiving prop)
    0xF3FE519E30A7A34AULL, // polySurfaceShape138 (Thanksgiving prop)
    0xF3C9017B6F8FD76BULL, // hornofplentyShape
    0x4203CA6FE7946F26ULL, // pCubeShape17 (newspaper)
    0x3CD091202B8032AFULL
    };
    return SHARFrontendMenuUIDInList(
        drawable, outlineUIDs,
        sizeof(outlineUIDs) / sizeof(outlineUIDs[0]));
}

bool IsSHARFrontendMenuFoliageDrawable(tDrawable* drawable)
{
    if (drawable == NULL)
        return false;

    // Preserve the FE plant-card treatment and the existing seasonal tree IDs.
    static const unsigned long long foliageUIDs[] =
    {
    0xA0F0AA1539C3022DULL,
    0x454F14FC392A8923ULL,
    0x454F14FC392A8922ULL,
    0x454F14FC392A8921ULL,
    0x454F14FC392A8920ULL,
    0x454F14FC392A8927ULL,
    0x454F14FC392A8926ULL,
    0x454F14FC392A8925ULL,
    0xA0F0AA1539C3022EULL,
    0xA0F0AA1539C3022FULL,
    0xA0F0AA1539C30228ULL,
    0xA0F0AA1539C30229ULL,
    0xA0F0AA1539C3022AULL,
    0xA0F0AA1539C3022BULL,
    0xA0F0AA1539C30224ULL,
    0xA0F0AA1539C30225ULL,
    0x95F914936F1B7374ULL,
    0xB1609118CB7D2F25ULL,
    0xD05AEC4B35673406ULL
    };
    return SHARFrontendMenuUIDInList(
        drawable, foliageUIDs,
        sizeof(foliageUIDs) / sizeof(foliageUIDs[0]));
}

static void DisplaySHARFrontendMenuDrawable(tDrawable* drawable)
{
    if (drawable == NULL)
        return;

    if (!IsCelShadingEnabled())
    {
        drawable->Display();
        return;
    }

    const bool foliageDrawable = IsSHARFrontendMenuFoliageDrawable(drawable);
    SetSHAREnvironmentCelPassActive(true);
    SetSHAREnvironmentFoliageMeshPassActive(foliageDrawable);
    SetCelShadingObjectEnabled(true);

    SetCelShadingOutlinePass(true);
    drawable->Display();

    SetCelShadingOutlinePass(false);
    SetSHAREnvironmentFoliageMeshPassActive(false);
    drawable->Display();

    SetCelShadingObjectEnabled(false);
    SetSHAREnvironmentCelPassActive(false);
}
#endif

namespace Scenegraph
{
static rmt::Matrix identity(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);
static rmt::Matrix tmp;

// Scenegraph  -----------------------------------------------
Scenegraph::Scenegraph() : root(NULL)
{
    boundingBox.high.Set( -FLT_MAX, -FLT_MAX, -FLT_MAX );
    boundingBox.low.Set( FLT_MAX,  FLT_MAX,  FLT_MAX );
}

Scenegraph::~Scenegraph() 
{
    tRefCounted::Release(root);
}

void Scenegraph::SetRoot(Node* n) 
{ 
    tRefCounted::Assign(root,n);
}

void Scenegraph::UpdateTransform(void)
{
    if(root->flags & Node::BRANCH_TRANSFORM_DIRTY)
    {
        root->UpdateTransform(identity);
    }
}

void Scenegraph::Display(void) 
{ 
    UpdateTransform();

    p3d::stack->Push();
    rmt::Matrix tmp = *p3d::stack->GetMatrix();
    root->Display(tmp, translucentDrawables);
    translucentDrawables.Display();  
    p3d::stack->Pop();
}

void Scenegraph::CustomDisplay(CustomDisplayCallback* callback) 
{ 
    UpdateTransform();

    p3d::stack->Push();
    rmt::Matrix tmp = *p3d::stack->GetMatrix();
    root->CustomDisplay(tmp, callback);
    p3d::stack->Pop();
}

Node* Scenegraph::Find(const char* name)
{
    return Search(root, tEntity::MakeUID(name));
}

Node* Scenegraph::Search(Node* node, tUID uid)
{
    if(!node)
        return NULL;

    if(node->GetUID() == uid)
        return node;

    unsigned count = node->GetNumChildren();
    for(unsigned i = 0; i < count; i++)
    {
        Node* node2 = Search(node->GetChild(i), uid);

        if(node2)
            return node2;
    }

    return NULL;
}

class ShaderVisitor : public SceneGraphVisitor
{
public:
    ShaderVisitor(tDrawable::ShaderCallback& c) : callback(c) {}
    virtual void Visit(Node* sceneGraphNode)
    {
        Drawable* drawable = dynamic_cast<Drawable*>(sceneGraphNode);
        if(drawable)
        {
            if(drawable->GetDrawable())
            {
                drawable->GetDrawable()->ProcessShaders(callback);
            }
        }
    }
protected:
    tDrawable::ShaderCallback& callback;
};

void Scenegraph::ProcessShaders(ShaderCallback& callback)
{

    ShaderVisitor v(callback);
    SceneGraphTraversal::Traverse(root, v);
}

void DrawableCounter::Visit(Node* sceneGraphNode)
{
    Drawable* drawable = dynamic_cast<Drawable*>(sceneGraphNode);
    if(drawable)
    {
        nDrawables++;
    }
}
void TranslucentDrawableCounter::Visit(Node* sceneGraphNode)
{
    Drawable* drawable = dynamic_cast<Drawable*>(sceneGraphNode);
    if(drawable && drawable->IsTranslucent())
    {
        nDrawables++;
    }
}

void SceneGraphTraversal::Traverse(Node* node, SceneGraphVisitor& vis)
{
    unsigned count = node->GetNumChildren();

    // Visit the node
    vis.Visit(node);
    for(unsigned i = 0; i < count; i++)
    {
        Traverse(node->GetChild(i), vis);
    }
}

Iterator::Iterator(Scenegraph* scenegraph)
:
m_scenegraph(NULL),
m_current(NULL),
m_isDone(false),
m_currentStackIndex(0),
m_searchStackSize(0),
m_searchStack(NULL)
{
    m_searchStackSize = 200; // this is temporary
    m_searchStack = new SearchState[m_searchStackSize];
    tRefCounted::Assign(m_scenegraph, scenegraph);
}

Iterator::~Iterator()
{
    delete [] m_searchStack;
    tRefCounted::Release(m_scenegraph);
}

void Iterator::First ()
{
    m_currentStackIndex = 0;
    m_current = m_scenegraph->GetRoot();
    m_searchStack[m_currentStackIndex].sceneNode = m_current;
    m_searchStack[m_currentStackIndex].currentChildIndex = 0;
    m_isDone = false;
}

void Iterator::Next ()
{
    P3DASSERT(m_currentStackIndex<m_searchStackSize);

    Node* currentParentNode = m_searchStack[m_currentStackIndex].sceneNode;
    unsigned currentChildCount  = m_searchStack[m_currentStackIndex].currentChildIndex;

    //
    // Are we Done?
    //
    if((m_currentStackIndex == 0) && (currentChildCount > currentParentNode->GetNumChildren()-1))
    {
        this->m_isDone = true;
        return;
    }
    else if(currentChildCount<currentParentNode->GetNumChildren())
    {
        //
        // increment the parents child search
        //
        m_searchStack[m_currentStackIndex].currentChildIndex++;

        //
        // Get the Next Child and push its context
        //        
        m_currentStackIndex++; 
        m_searchStack[m_currentStackIndex].sceneNode = currentParentNode->GetChild(currentChildCount);
        m_searchStack[m_currentStackIndex].currentChildIndex = 0;
        m_current = m_searchStack[m_currentStackIndex].sceneNode;
    }
    else if(m_currentStackIndex>0)
    {
        //
        // No more children to find so pop
        //
        while(currentChildCount>=currentParentNode->GetNumChildren() && m_currentStackIndex>0)
        {
            m_searchStack[m_currentStackIndex].sceneNode = NULL;
            m_searchStack[m_currentStackIndex].currentChildIndex = 0;
            --m_currentStackIndex;
            currentParentNode = m_searchStack[m_currentStackIndex].sceneNode;
            currentChildCount  = m_searchStack[m_currentStackIndex].currentChildIndex;
        }

        if((m_currentStackIndex>=0) && (currentChildCount < currentParentNode->GetNumChildren()))
        {
            m_current = currentParentNode->GetChild(currentChildCount);
            m_searchStack[m_currentStackIndex].currentChildIndex++;
        }
    }
}

void  LightGroupIterator::First(void)
{
    // assume that the first node is NEVER a lightGroup
    Iterator::First();// initialize
    Next();
}

void  LightGroupIterator::Next(void)
{
    LightGroup* lightNode = NULL;
    do
    {
        Iterator::Next();
        lightNode = dynamic_cast<LightGroup*>(m_current);

    }while(lightNode == NULL && !m_isDone);
}

LightGroup* LightGroupIterator::GetLightGroup(void)
{
    return(dynamic_cast<LightGroup*>(m_current));
}


// Node  -----------------------------------------------

Node::Node() : parent(NULL), flags(NODE_TRANSFORM_DIRTY | BRANCH_TRANSFORM_DIRTY)
{
}

Node::~Node() 
{
}

void Node::SetParent(Node* p) 
{
    parent = p; // weak reference, dont ref count it
}

Node*  Node::GetParent() 
{
    return parent; 
}

const rmt::Matrix& Node::GetTransform(void) 
{
    if(parent)
        return parent->GetTransform();
    else
        return identity;
}

const rmt::Matrix& Node::GetWorldTransform(void) 
{
    flags &= ~NODE_TRANSFORM_DIRTY;

    if(parent)
        return parent->GetWorldTransform();
    else
        return identity;
}

void Node::SetFlags(bool set, unsigned f, Propagation prop, bool stopIfSet)
{
    if((prop == PROPAGATE_UP) && parent)
    {
        if(set)
        {
            if(stopIfSet && ((flags & f) == f))
                return;
        }
        else
        {
            if(stopIfSet && ((flags & f) == 0))
                return;
        }
        parent->SetFlags(set,f,prop, stopIfSet);
    }

    if(set)
    {
        flags |= f;
    }
    else
    {
      flags &= ~f;
    }

}

// Branch  -----------------------------------------------

Branch::Branch(int initialChildren) : nChildren(0), allocatedChildren(initialChildren), children(NULL) 
{

    if(allocatedChildren)
    {
        children = new Node*[allocatedChildren];
    }
    else
    {
//        p3d::printf("Warning : Scenegraph branch '%s' has no children. It should be removed.\n", this->GetName());
    }

    for(int i = 0; i < allocatedChildren; i++)
        children[i] = NULL;
}

Branch::~Branch() 
{
    for(int i = 0; i < allocatedChildren; i++)
    {
        if(children[i])
            children[i]->SetParent(NULL);

        tRefCounted::Release(children[i]);
    }

    delete [] children;
}

unsigned Branch::GetNumChildren(void) 
{ 
    return nChildren; 
}

Node* Branch::GetChild(unsigned index) 
{ 
    unsigned count = 0;
    for(int i = 0; i < allocatedChildren; i++)
    {
        if(children[i])
        {
            if(count == index)
                return children[i];
            else
                count++;
        }
    }
    return NULL;
}

void Branch::AddChild(Node* child) 
{
    if(!child)
        return;

    if(nChildren == allocatedChildren)
        Resize(allocatedChildren*2);

    for(int i = 0; i < allocatedChildren; i++)
    {
        if(!children[i])
        {
            tRefCounted::Assign(children[i], child);
            child->SetParent(this);
            nChildren++;
            return;
        }
    }
}

void Branch::RemoveChild(Node* child) 
{
    for(int i = 0; i < allocatedChildren; i++)
    {
        if(children[i])
        {
            if(children[i] == child)
            {
                children[i]->SetParent(NULL);
                tRefCounted::Release(children[i]);
                nChildren--;
                return;
            }
        }
    }
}

void Branch::Display(const rmt::Matrix& view, DisplayList& list) 
{
    for(int i = 0; i < allocatedChildren; i++)
        if(children[i]) children[i]->Display(view, list);
}

void Branch::CustomDisplay(const rmt::Matrix& view, CustomDisplayCallback* callback) 
{
    for(int i = 0; i < allocatedChildren; i++)
        if(children[i]) children[i]->CustomDisplay(view, callback);
}

void Branch::UpdateTransform(const rmt::Matrix& parent) 
{
    for(int i = 0; i < allocatedChildren; i++)
    {
        if(children[i]) 
        {
            if(children[i]->GetFlags() & BRANCH_TRANSFORM_DIRTY)
                children[i]->UpdateTransform(parent);
        }
    }
    flags &= ~BRANCH_TRANSFORM_DIRTY;
}

void Branch::SetFlags(bool set, unsigned f, Propagation prop, bool stopIfSet)
{
    if(prop == PROPAGATE_DOWN)
    {
        if(set)
        {
            if(stopIfSet && ((flags & f) == f))
                return;
        }
        else
        {
            if(stopIfSet && ((flags & f) == 0))
                return;
        }

        for(int i = 0; i < allocatedChildren; i++)
            if(children[i]) children[i]->SetFlags(set,f,prop, stopIfSet);
    }

    Node::SetFlags(set,f,prop, stopIfSet);
}


void Branch::Resize(int n)
{
    allocatedChildren = n;
    Node** newChildren= new Node*[allocatedChildren];
    memset(newChildren,0, sizeof(int)*allocatedChildren);
    memcpy(newChildren,children, sizeof(Node*)*nChildren);
    delete [] children;
    children = newChildren;
}


// Leaf  -----------------------------------------------

Leaf::Leaf() 
{
}

Leaf::~Leaf() 
{
}

unsigned Leaf::GetNumChildren(void) 
{
    return 0; 
}

Node* Leaf::GetChild(unsigned i) 
{
    return NULL;
}

void Leaf::UpdateTransform(const rmt::Matrix& parent)
{
}

void Leaf::Display(const rmt::Matrix& view, DisplayList& list)
{
}

void Leaf::CustomDisplay(const rmt::Matrix& view, CustomDisplayCallback*)
{
}


// Transform  -----------------------------------------------
Transform::Transform(int n) : Branch(n)
{
    transform.Identity();
    world.Identity();
}

Transform::~Transform() 
{
}

const rmt::Matrix& Transform::GetTransform(void)
{
    return transform;
}

const rmt::Matrix& Transform::GetWorldTransform(void)
{
    if((flags & NODE_TRANSFORM_DIRTY) && !(flags & USER_WORLD_TRANSFORM))
    {
        world.Mult(transform, parent->GetWorldTransform());
        flags &= ~NODE_TRANSFORM_DIRTY;
    }

    return world;
}

rmt::Matrix* Transform::ModifyTransform(void)
{
    SetFlags(true, NODE_TRANSFORM_DIRTY | BRANCH_TRANSFORM_DIRTY, PROPAGATE_DOWN, true);
    if(parent)
        parent->SetFlags(true, BRANCH_TRANSFORM_DIRTY, PROPAGATE_UP, true);
    return &transform;
}

rmt::Matrix* Transform::ModifyWorldTransform(void)
{
    return &world;
}

void Transform::SetTransform(const rmt::Matrix& t)
{
    transform = t;
    SetFlags(true, NODE_TRANSFORM_DIRTY | BRANCH_TRANSFORM_DIRTY, PROPAGATE_DOWN, true);
    if(parent)
        parent->SetFlags(true, BRANCH_TRANSFORM_DIRTY, PROPAGATE_UP, true);
}

void Transform::SetWorldTransform(const rmt::Matrix& w)
{
    world = w;
}

void Transform::Display(const rmt::Matrix& view, DisplayList& list) 
{
    tmp.Mult(world, view);
    p3d::stack->PushLoad(tmp);
    Branch::Display(view, list);
    p3d::stack->Pop();
}

void Transform::CustomDisplay(const rmt::Matrix& view, CustomDisplayCallback* callback) 
{
    tmp.Mult(world, view);
    p3d::stack->PushLoad(tmp);
    Branch::CustomDisplay(view, callback);
    p3d::stack->Pop();
}

void Transform::UpdateTransform(const rmt::Matrix& parent)
{
    if(!(flags & USER_WORLD_TRANSFORM))
        world.Mult(transform, parent);

    flags &= ~NODE_TRANSFORM_DIRTY;
    Branch::UpdateTransform(world);
}


// Drawable  -----------------------------------------------
Drawable::Drawable() : draw(NULL), alternateDraw(NULL), hasAlternateTransform(false), isTranslucent(false), sortOrder(0.5f)
{
    // ...
}

Drawable::Drawable(char* name) : draw(NULL), alternateDraw(NULL), hasAlternateTransform(false), isTranslucent(false), sortOrder(0.5f) 
{
    tDrawable* d = p3d::find<tDrawable>(name);
    tRefCounted::Assign(draw, d);
}

Drawable::Drawable(tDrawable* d) : draw(NULL), alternateDraw(NULL), hasAlternateTransform(false), isTranslucent(false), sortOrder(0.5f) 
{
    tRefCounted::Assign(draw, d);
}

void Drawable::SetDrawable(tDrawable* d) 
{
        tRefCounted::Assign(draw, d);
}

void Drawable::SetAlternateDrawable(tDrawable* d)
{
    tRefCounted::Assign(alternateDraw, d);
}

void Drawable::SetAlternateDrawableTransform(const rmt::Matrix& transform)
{
    alternateTransform = transform;
    hasAlternateTransform = true;
}

Drawable::~Drawable() 
{
    tRefCounted::Release(draw);
    tRefCounted::Release(alternateDraw);
}

void Drawable::Display(const rmt::Matrix&, DisplayList& list)
{
    if (draw)
    {
#ifdef RAD_ANDROID
        const int celMode = GetSHARCelShadingMode();
        const bool usingAlternateDrawable =
            alternateDraw != NULL && celMode != 0;
        tDrawable* drawableToDisplay =
            usingAlternateDrawable ? alternateDraw : draw;
        const bool applyAlternateTransform =
            usingAlternateDrawable && hasAlternateTransform;
        if (applyAlternateTransform)
            p3d::stack->PushMultiply(alternateTransform);

        const unsigned long long drawableUID =
            static_cast<unsigned long long>(drawableToDisplay->GetUID());
        const bool gagOutlinePass =
            IsSHARGagCharacterCelPassActive() &&
            IsCelShadingEnabled() && !isTranslucent;
        if (!isTranslucent)
        {
            SetSHARCurrentDrawableUID(drawableUID);
        }
#endif
        if (isTranslucent)
        {
            // The alternate's local alignment is already on the matrix stack.
#ifdef RAD_ANDROID
            list.Add(drawableToDisplay, NULL, sortOrder);
#else
            list.Add(draw, NULL, sortOrder);
#endif
        }
        else
        {
#ifdef RAD_ANDROID
            if (gagOutlinePass)
            {
                SetSHARCharacterMaskModelPass(true);
                SetCelShadingObjectEnabled(true);
                SetCelShadingOutlinePass(true);
                drawableToDisplay->Display();
                SetCelShadingOutlinePass(false);
                drawableToDisplay->Display();
                SetCelShadingObjectEnabled(false);
                SetSHARCharacterMaskModelPass(false);
            }
            else if (IsSHARFrontendMenuOutlineDrawable(drawableToDisplay))
                DisplaySHARFrontendMenuDrawable(drawableToDisplay);
            else
                drawableToDisplay->Display();
            ClearSHARCurrentDrawableUID();
#else
            draw->Display();
#endif
        }
#ifdef RAD_ANDROID
        if (applyAlternateTransform)
            p3d::stack->Pop();
#endif
    }
}

void Drawable::CustomDisplay(const rmt::Matrix& mat, CustomDisplayCallback* callback) 
{
    callback->Display(this);
}


// Transform  -----------------------------------------------

Attachment::Attachment()
{
}

Attachment::Attachment(tDrawablePose* p, int initialChildren) : Branch(initialChildren), pose(NULL)
{
    tRefCounted::Assign(pose, p);
    jointIndices = new int[allocatedChildren];
}

void Attachment::SetDrawablePose(tDrawablePose* p)
{
    tRefCounted::Assign(pose, p);
}

Attachment::~Attachment()
{
    tRefCounted::Release(pose);
    delete [] jointIndices;
}

tDrawablePose* Attachment::GetDrawablePose()
{
    return pose;
}

Node* Attachment::GetAttachment(int n)
{
    return Branch::GetChild(n);
}

int   Attachment::GetAttachmentJoint(int n)
{
    int count = 0;
    for(int i = 0; i < allocatedChildren; i++)
    {
        if(children[i])
        {
            if(count == n)
                return jointIndices[i];
            else
                count++;
        }
    }
    return 0;
}

void Attachment::AddChild(Node* child)
{
    SetAttachment(~0, child);
}

void Attachment::SetAttachment(int joint, Node* graft)
{
    if(!graft)
        return;

    if(nChildren == allocatedChildren)
        Resize(allocatedChildren*2);

    for(int i = 0; i < allocatedChildren; i++)
    {
        if(!children[i])
        {
            Transform* transform = new Transform(1);
            transform->AddChild(graft);
            Node* t = transform;

            tRefCounted::Assign(children[i], t);
            children[i]->SetParent(this);
            jointIndices[i] = joint;
            nChildren++;
            return;
        }
    }
}

void Attachment::RemoveAttachment(Node* graft)
{
    RemoveChild(graft);
}

void Attachment::Display(const rmt::Matrix& view, DisplayList& list)
{
    if(pose)
    {
        pose->Display();
    }
    Branch::Display(view, list);

    // HACK : can't tell if attachment is evaluated or not, so force a recalc
    SetFlags(true, NODE_TRANSFORM_DIRTY | BRANCH_TRANSFORM_DIRTY, PROPAGATE_DOWN, true);
    if(parent)
        parent->SetFlags(true, BRANCH_TRANSFORM_DIRTY, PROPAGATE_UP, true);

}

void Attachment::EvaluateAttachments(void)
{
    if(!pose)
        return;

    pose->GetPose()->Evaluate();
    for(int i = 0; i < allocatedChildren; i++)
    {
        if(children[i])
        {
            Transform* t = dynamic_cast<Transform*>(children[i]);
            P3DASSERT(t);
            *t->ModifyTransform() = pose->GetPose()->GetJoint(jointIndices[i])->worldMatrix;
             
        }
    }
}

void Attachment::UpdateTransform(const rmt::Matrix& parent)
{
    EvaluateAttachments();
    Branch::UpdateTransform(parent);
}

void Attachment::Resize(int n)
{
    Branch::Resize(n);
    int* newJoints = new int[allocatedChildren];
    memcpy(newJoints,jointIndices, sizeof(int)*nChildren);
    delete [] jointIndices;
    jointIndices = newJoints;
}

// Visibility  -----------------------------------------------
Visibility::Visibility(int initialChildren):isVisible(true)
{

}

void Visibility::Display(const rmt::Matrix& m, DisplayList& list)
{
    if(isVisible)
    {
        Branch::Display(m, list);
    }
}

void Visibility::CustomDisplay(const rmt::Matrix& m, CustomDisplayCallback* callback)
{
    if(isVisible)
    {
        Branch::CustomDisplay(m, callback);
    }
}

// Camera  -----------------------------------------------
Camera::Camera() : camera(NULL)
{
}

Camera::Camera(tCamera* c) : camera(NULL)
{
    tRefCounted::Assign(camera, c);
}

Camera::~Camera()
{
    tRefCounted::Release(camera);
}

tCamera* Camera::GetCamera()
{
    return camera;
}

void Camera::SetCamera(tCamera* c)
{
    tRefCounted::Assign(camera, c);
}

LightGroup::LightGroup() : lights(NULL)
{
}

LightGroup::LightGroup(tLightGroup* l) : lights(NULL)
{
    tRefCounted::Assign(lights, l);
}

LightGroup::~LightGroup()
{
    tRefCounted::Release(lights);

}

tLightGroup* LightGroup::GetLights()
{
    return lights;
}

void LightGroup::SetLights(tLightGroup* l)
{   
    tRefCounted::Assign(lights, l);
}

// Loaders  -----------------------------------------------
static char buf[256];
static char buf2[256];
Loader::Loader() : tSimpleChunkHandler(Pure3D::SceneGraph::SCENEGRAPH)
{
}

tEntity* Loader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    return(bothFormatsLoader.LoadObject(f, store));
}


//
// These are essentially static functions.
//
tEntity* SceneGraphGenericLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    f->GetPString(buf);
    unsigned version = f->GetLong();
    P3DASSERT(version == 0);

    Scenegraph* scene = new Scenegraph;
    scene->SetName(buf);
#ifdef RAD_ANDROID
    // Only remap the standard living-room picture on the base camset.
    const bool isFrontendCamset =
        static_cast<unsigned long long>(tEntity::MakeUID(buf)) ==
        0xB44F7CB2EB3385D0ULL;
#else
    const bool isFrontendCamset = false;
#endif

    //
    // Clear bounding box for loader
    //
    boundingBox.high.Set( -FLT_MAX, -FLT_MAX, -FLT_MAX );
    boundingBox.low.Set( FLT_MAX,  FLT_MAX,  FLT_MAX );

	rmt::Matrix root;
	root.Identity();

    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
            case Pure3D::SceneGraph::ROOT:
                {
                    f->BeginChunk();
                    scene->SetRoot(LoadNode(f,store, &root, isFrontendCamset));
                    f->EndChunk();
                }
                break;

            default:
                break;
        }
        f->EndChunk();
    }
    
    //
    // Scene Graph drawList setup
    //
#ifdef RAD_ANDROID
    // Keep the authored picture node in place. Only the alternate replacement
    // receives the alignment transform, when Lite or Full selects that resource.
    if (isFrontendCamset)
    {
        Drawable* pictureFrame = dynamic_cast<Drawable*>(
            scene->Find("FE_picture_frameShape1"));
        Drawable* replacementPicture = dynamic_cast<Drawable*>(
            scene->Find("FE_pCubeShape23"));

        if (pictureFrame != NULL &&
            replacementPicture != NULL &&
            replacementPicture->GetDrawable() != NULL)
        {
            const rmt::Matrix pictureFrameAlignment(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.22134346f, -0.12115437f, 5.20387244f, 1.0f);

            pictureFrame->SetAlternateDrawable(
                replacementPicture->GetDrawable());
            pictureFrame->SetAlternateDrawableTransform(
                pictureFrameAlignment);
        }
        else
        {
        }
    }
#endif

    TranslucentDrawableCounter translucentDrawableCount;
    SceneGraphTraversal::Traverse(scene->GetRoot(), translucentDrawableCount);
    scene->translucentDrawables.SetSize(translucentDrawableCount.GetCount());

    //
    // Copy calculated bounding box to scenegraph
    //
    scene->boundingBox = boundingBox;
    
    return scene;
}

Node* SceneGraphGenericLoader::LoadNode(tChunkFile* f, tEntityStore* store, rmt::Matrix* Root, bool isFrontendCamset)
{
    tDrawablePose* drawablePose;
    tDrawable* loadedDrawable;
    rmt::Box3D drawableBox;

    switch(f->GetCurrentID())
    {
        case Pure3D::SceneGraph::BRANCH:
        {
            f->GetPString(buf);
            unsigned n = f->GetLong();
            Branch* branch = new Branch(n);
            branch->SetName(buf);

            for(unsigned i = 0; i < n; i++)
            {
                f->BeginChunk();
                branch->AddChild(LoadNode(f,store, Root, isFrontendCamset));
                f->EndChunk();
            }

            return branch;
        }
        
        case Pure3D::SceneGraph::TRANSFORM:
        {
            f->GetPString(buf);
            unsigned n = f->GetLong();
            Transform* branch = new Transform(n);
            branch->SetName(buf);
            f->GetData(branch->ModifyTransform(), 16, tFile::DWORD);
			rmt::Matrix newRoot;
			if(Root)
			{
				newRoot.Mult(*Root, branch->GetTransform());
			}
			else
			{
				newRoot.Identity();
			}
            for(unsigned i = 0; i < n; i++)
            {
                f->BeginChunk();
                branch->AddChild(LoadNode(f,store, &newRoot, isFrontendCamset));
                f->EndChunk();
            }

            return branch;
        }

        case Pure3D::SceneGraph::ATTACHMENT:
        {
            f->GetPString(buf);
            f->GetPString(buf2);

            unsigned n = f->GetLong();
            drawablePose = p3d::find<tDrawablePose>(store,buf2);
            Attachment* graft = new Attachment(drawablePose, n);
            graft->SetName(buf);

            //
            // Modify bounding box to enclose box for pose
            //
            if( drawablePose != NULL )
            {
                drawablePose->GetBoundingBox( &drawableBox );
				if(Root)
				{
					Root->Transform(drawableBox.high, &drawableBox.high);
					Root->Transform(drawableBox.low, &drawableBox.low);
				}
                boundingBox.high.x = rmt::Max( (drawableBox.high.x), (boundingBox.high.x) );
                boundingBox.high.y = rmt::Max( (drawableBox.high.y), (boundingBox.high.y) );
                boundingBox.high.z = rmt::Max( (drawableBox.high.z), (boundingBox.high.z) );

                boundingBox.low.x = rmt::Min( (drawableBox.low.x), (boundingBox.low.x) );
                boundingBox.low.y = rmt::Min( (drawableBox.low.y), (boundingBox.low.y) );
                boundingBox.low.z = rmt::Min( (drawableBox.low.z), (boundingBox.low.z) );
            }

            for(unsigned i = 0; i < n; i++)
            {
                f->BeginChunk();
                long CurrentID = f->GetCurrentID();
                P3DASSERT(CurrentID == Pure3D::SceneGraph::ATTACHMENTPOINT);
                int joint = f->GetLong();

                f->BeginChunk();
                graft->SetAttachment(joint,LoadNode(f,store, Root, isFrontendCamset));
                f->EndChunk();
                f->EndChunk();
            }

            return graft;
        }

        case Pure3D::SceneGraph::VISIBILITY:
        {
            f->GetPString(buf);
            unsigned n = f->GetLong();
            Visibility* branch = new Visibility(n);
            branch->SetName(buf);
            branch->SetVisibility(f->GetLong()?true:false);

            for(unsigned i = 0; i < n; i++)
            {
                f->BeginChunk();
                branch->AddChild(LoadNode(f,store, Root, isFrontendCamset));
                f->EndChunk();
            }

            return branch;
        }

        case Pure3D::SceneGraph::DRAWABLE:
        {
            f->GetPString(buf);
            f->GetPString(buf2);
            bool isTranslucent = (f->GetLong()==1)?true:false;
            
            // Resolve the authored drawable resource normally. The frontend
            // picture-frame replacement is assigned after the complete
            // scenegraph has loaded, using the original node's actual resource.
            loadedDrawable = p3d::find<tDrawable>(store, buf2);
            Drawable* drawable = new Drawable(loadedDrawable);
            drawable->SetName(buf);
            drawable->SetTranslucent(isTranslucent);
            //
            // Modify bounding box to enclose box for drawable
            //
            if( loadedDrawable != NULL )
            {
                loadedDrawable->GetBoundingBox( &drawableBox );
				if(Root)
				{
					Root->Transform(drawableBox.high, &drawableBox.high);
					Root->Transform(drawableBox.low, &drawableBox.low);
				}
                boundingBox.high.x = rmt::Max( (drawableBox.high.x), (boundingBox.high.x) );
                boundingBox.high.y = rmt::Max( (drawableBox.high.y), (boundingBox.high.y) );
                boundingBox.high.z = rmt::Max( (drawableBox.high.z), (boundingBox.high.z) );

                boundingBox.low.x = rmt::Min( (drawableBox.low.x), (boundingBox.low.x) );
                boundingBox.low.y = rmt::Min( (drawableBox.low.y), (boundingBox.low.y) );
                boundingBox.low.z = rmt::Min( (drawableBox.low.z), (boundingBox.low.z) );
            }

            while(f->ChunksRemaining())
            {
                switch(f->BeginChunk())
                {
                    case Pure3D::SceneGraph::SORTORDER:
                        {
                            drawable->SetSortOrder(f->GetFloat());
                        }
                        break;

                    default:
                        break;
                }
                f->EndChunk();
            }

            return drawable;
        }

        case Pure3D::SceneGraph::CAMERA:
        {
            f->GetPString(buf);
            f->GetPString(buf2);
            
            Camera* camera = new Camera(p3d::find<tCamera>(store,buf2));
            camera->SetName(buf);
            return camera;
        }
        
        case Pure3D::SceneGraph::LIGHTGROUP:
        {
            f->GetPString(buf);
            f->GetPString(buf2);
            
            LightGroup* lightGroup = new LightGroup(p3d::find<tLightGroup>(store,buf2));
            lightGroup->SetName(buf);
            return lightGroup;
        }
    }
    return NULL;
}

} // end namespace Scenegraph

