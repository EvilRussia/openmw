#include "viewdata.hpp"

namespace Terrain
{

ViewData::ViewData()
    : mNumEntries(0)
    , mFrameLastUsed(0)
    , mChanged(false)
    , mPersistent(false)
{

}

ViewData::~ViewData()
{

}

void ViewData::add(QuadTreeNode *node, bool visible)
{
    unsigned int index = mNumEntries++;

    if (index+1 > mEntries.size())
        mEntries.resize(index+1);

    Entry& entry = mEntries[index];
    if (entry.set(node, visible))
        mChanged = true;
}

unsigned int ViewData::getNumEntries() const
{
    return mNumEntries;
}

ViewData::Entry &ViewData::getEntry(unsigned int i)
{
    return mEntries[i];
}

bool ViewData::hasChanged() const
{
    return mChanged;
}

void ViewData::reset(unsigned int frame)
{
    // clear any unused entries
    for (unsigned int i=mNumEntries; i<mEntries.size(); ++i)
        mEntries[i].set(NULL, false);

    // reset index for next frame
    mNumEntries = 0;
    mChanged = false;

    mFrameLastUsed = frame;
}

void ViewData::clear()
{
    for (unsigned int i=0; i<mEntries.size(); ++i)
        mEntries[i].set(NULL, false);
    mNumEntries = 0;
    mFrameLastUsed = 0;
    mChanged = false;
}

bool ViewData::contains(QuadTreeNode *node)
{
    for (unsigned int i=0; i<mNumEntries; ++i)
        if (mEntries[i].mNode == node)
            return true;
    return false;
}

ViewData::Entry::Entry()
    : mNode(NULL)
    , mVisible(true)
    , mLodFlags(0)
{

}

bool ViewData::Entry::set(QuadTreeNode *node, bool visible)
{
    mVisible = visible;
    if (node == mNode)
        return false;
    else
    {
        mNode = node;
        // clear cached data
        mRenderingNode = NULL;
        return true;
    }
}

ViewData *ViewDataMap::getViewData(osg::Object *viewer, bool ref)
{
    Map::const_iterator found = mViews.find(viewer);
    if (found == mViews.end())
    {
        ViewData* vd = createOrReuseView();
        if (ref)
            vd->setViewer(viewer);
        mViews[viewer] = vd;
        return vd;
    }
    else
        return found->second;
}

ViewData *ViewDataMap::createOrReuseView()
{
    if (mUnusedViews.size())
    {
        ViewData* vd = mUnusedViews.front();
        mUnusedViews.pop_front();
        return vd;
    }
    else
    {
        mViewVector.push_back(ViewData());
        return &mViewVector.back();
    }
}

void ViewDataMap::removeView(ViewData* vd)
{
    vd->setPersistent(false);
    vd->clear();
    mUnusedViews.push_back(vd);
}

void ViewDataMap::clearUnusedViews(unsigned int frame)
{
    for (Map::iterator it = mViews.begin(); it != mViews.end(); )
    {
        ViewData* vd = it->second;
        if (!vd->getPersistent() &&
                (!vd->getViewer() // if no ref was held, always need to clear to avoid holding a dangling ref.
                || vd->getFrameLastUsed() + 2 < frame))
        {
            vd->setViewer(NULL);
            vd->clear();
            mUnusedViews.push_back(vd);
            mViews.erase(it++);
        }
        else
            ++it;
    }
}

void ViewDataMap::clear()
{
    mViews.clear();
    mUnusedViews.clear();
    mViewVector.clear();
}


}
