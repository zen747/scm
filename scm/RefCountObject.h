#ifndef RefCountObject_H
#define RefCountObject_H

namespace scm {
/** RefcountObject
 * 關於如何使用retain(), release(), autrelease(), 請參考Apple的Objective-C文件。
 * Please refer to Apple's Objective-C document for how to use retain(), release(), and autorelease().
*/

class RefCountObject
{
private:

    int ref_count_;

public:
    RefCountObject ();
    /** \brief ref_count_ + 1 */
    void retain ();
    /** \brief ref_count_ - 1。 若 ref_count_ 變成0會解構物件. If ref_count_ down to 0, object will be destructed. */
    void release ();
    /** \brief 將物件加到 release queue 裏， 由AutoReleasePool管理。 Add object to release queue, managed by AutoReleasePool. */
    void autorelease ();

    int ref_count () const {
        return ref_count_;
    }
    bool unique_ref () const {
        return ref_count_ == 1;
    }

protected:
    virtual ~RefCountObject ();

private:
    RefCountObject (RefCountObject const&rhs) {}
    RefCountObject &operator=(RefCountObject const&rhs) {
        return *this;
    }
};

class RefCountObjectGuard
{
    RefCountObject *obj_;
public:
    RefCountObjectGuard (RefCountObject *obj=0);
    ~RefCountObjectGuard ();
    RefCountObjectGuard (RefCountObjectGuard const&rhs);
    RefCountObjectGuard &operator=(RefCountObjectGuard const&rhs);
    RefCountObjectGuard &operator=(RefCountObject *obj);
    RefCountObject *operator-> () {
        return obj_;
    }
    void reset (RefCountObject *obj=0);
};

/** 呼叫了 autorelease 的物件會被放在這邊， 呼叫AutoReleasePool::pumpPools時無人reference的物件會被解構。
 * Objects invoked autorelease() will be placed in AutoReleasePool, when AutoReleasePool::pumpPools() invoked, objects without references will be destructed.
 */
class AutoReleasePool
{
public:
    void addToAutoreleasePool (RefCountObject *obj);
    void pumpPool ();

    AutoReleasePool ();
    ~AutoReleasePool ();

    static AutoReleasePool &currentPool ();
    static void             pumpPools ();

private:
    struct PRIVATE;
    friend struct PRIVATE;
    PRIVATE *private_;

};

}


#endif
