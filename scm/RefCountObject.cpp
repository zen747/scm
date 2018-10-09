
#include "RefCountObject.h"
#include <cassert>
#include <vector>
#include <iostream>

using namespace std;

namespace scm {

RefCountObject::RefCountObject ()
    :ref_count_(1)
{
}

RefCountObject::~RefCountObject ()
{
}

void RefCountObject::retain ()
{
    ++ref_count_;
}

void RefCountObject::release ()
{
    assert (ref_count_ > 0);
    --ref_count_;
    if (ref_count_ == 0) {
        delete this;
    }
}

void RefCountObject::autorelease ()
{
    AutoReleasePool::currentPool().addToAutoreleasePool (this);
}


RefCountObjectGuard::RefCountObjectGuard (RefCountObject *obj)
    :obj_(obj)
{
    if (obj_) obj_->retain ();
}

RefCountObjectGuard::~RefCountObjectGuard ()
{
    if (obj_) obj_->release ();
}

RefCountObjectGuard::RefCountObjectGuard (RefCountObjectGuard const&rhs)
{
    if (rhs.obj_) rhs.obj_->retain ();
    obj_ = rhs.obj_;
}

RefCountObjectGuard &RefCountObjectGuard::operator=(RefCountObjectGuard const&rhs)
{
    if (rhs.obj_) rhs.obj_->retain ();
    if (obj_) obj_->release ();
    obj_ = rhs.obj_;
    return *this;
}

RefCountObjectGuard &RefCountObjectGuard::operator=(RefCountObject *obj)
{
    if (obj) obj->retain ();
    if (obj_) obj_->release ();
    obj_ = obj;
    return *this;
}

void RefCountObjectGuard::reset (RefCountObject *obj)
{
    if (obj) obj->retain ();
    if (obj_) obj_->release ();
    obj_ = obj;
}

struct AutoReleasePool::PRIVATE
{
    std::vector<RefCountObject *> objs_;
    static std::vector<AutoReleasePool *> pools_;
};

std::vector<AutoReleasePool *> AutoReleasePool::PRIVATE::pools_;

AutoReleasePool::AutoReleasePool()
{
    private_ = new PRIVATE;
    PRIVATE::pools_.push_back(this);
}

AutoReleasePool::~AutoReleasePool()
{
    assert (PRIVATE::pools_.back() == this);
    this->pumpPool();
    PRIVATE::pools_.pop_back();
    delete private_;
}

void AutoReleasePool::addToAutoreleasePool(RefCountObject* obj)
{
    assert (obj);
    assert (obj->ref_count () > 0);
    private_->objs_.push_back(obj);
}

AutoReleasePool& AutoReleasePool::currentPool()
{
    return *PRIVATE::pools_.back();
}

void AutoReleasePool::pumpPool()
{
    std::vector<RefCountObject *> objs;
    objs.swap(private_->objs_);
    for (size_t i=0; i < objs.size(); ++i) {
        objs[i]->release ();
    }
}

void AutoReleasePool::pumpPools()
{
    for (size_t i=0; i < PRIVATE::pools_.size(); ++i) {
        PRIVATE::pools_[i]->pumpPool();
    }
}



}

