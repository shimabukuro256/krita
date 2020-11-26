/*
 *  Copyright (c) 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __INPLACE_TRANSFORM_STROKE_STRATEGY_H
#define __INPLACE_TRANSFORM_STROKE_STRATEGY_H

#include <QObject>
#include <QMutex>
#include <QElapsedTimer>
#include <KoUpdater.h>
#include <kis_stroke_strategy_undo_command_based.h>
#include <kis_types.h>
#include "tool_transform_args.h"
#include <kis_processing_visitor.h>
#include <kritatooltransform_export.h>
#include <boost/optional.hpp>
#include <transform_transaction_properties.h>
#include "kis_selection_mask.h"
#include "KisAsyncronousStrokeUpdateHelper.h"
#include "kis_undo_stores.h"


class KisPostExecutionUndoAdapter;
class TransformTransactionProperties;
class KisUpdatesFacade;
class KisDecoratedNodeInterface;


class InplaceTransformStrokeStrategy : public QObject, public KisStrokeStrategyUndoCommandBased
{
    Q_OBJECT
public:
    class UpdateTransformData : public KisStrokeJobData {
    public:
        UpdateTransformData(ToolTransformArgs _args)
            : KisStrokeJobData(SEQUENTIAL, NORMAL),
              args(_args)
        {}

        KisStrokeJobData* createLodClone(int levelOfDetail) override {
            return new UpdateTransformData(*this, levelOfDetail);
        }

    private:
        UpdateTransformData(const UpdateTransformData &rhs, int levelOfDetail)
            : KisStrokeJobData(rhs),
              args(rhs.args)
        {
            Q_UNUSED(levelOfDetail);
        }

    public:
        ToolTransformArgs args;
    };

    struct KRITAUI_EXPORT BarrierUpdateData : public KisAsyncronousStrokeUpdateHelper::UpdateData
    {
        BarrierUpdateData(bool forceUpdate);
        KisStrokeJobData* createLodClone(int levelOfDetail) override;
    private:
        BarrierUpdateData(const BarrierUpdateData &rhs, int levelOfDetail);
    };


    enum CommandGroup {
        Clear = 0,
        ClearTemporary,
        Transform,
        TransformTemporary,
        TransformLod,
        TransformLodTemporary
    };

    struct SharedData {
        // initial conditions
        KisUpdatesFacade *updatesFacade;
        KisStrokeUndoFacade *undoFacade;
        ToolTransformArgs::TransformMode mode;
        bool workRecursively;
        QString filterId;
        bool forceReset;
        KisNodeSP rootNode;
        KisSelectionSP selection;
        KisNodeSP imageRoot;
        int previewLevelOfDetail = -1;
        bool forceLodMode = true;

        // properties filled by initialization stroke
        KisNodeList processedNodes;
        ToolTransformArgs initialTransformArgs;
        ToolTransformArgs currentTransformArgs;

        QList<KisSelectionSP> deactivatedSelections;
        KisSelectionMaskSP deactivatedOverlaySelectionMask;

        QMutex commandsMutex;

        QVector<std::pair<CommandGroup, KUndo2CommandSP>> commands;

        QMutex devicesCacheMutex;
        QHash<KisPaintDevice*, KisPaintDeviceSP> devicesCacheHash;
        QHash<KisTransformMask*, KisPaintDeviceSP> transformMaskCacheHash;

        QMutex dirtyRectsMutex;
        QHash<KisNodeSP, QRect> dirtyRects;
        QHash<KisNodeSP, QRect> prevDirtyRects;

        QHash<KisNodeSP, QRect> dirtyPreviewRects;
        QHash<KisNodeSP, QRect> prevDirtyPreviewRects;

        inline QHash<KisNodeSP, QRect>& effectiveDirtyRects(int levelOfDetail) {
            return levelOfDetail > 0 ? dirtyPreviewRects : dirtyRects;
        }

        inline QHash<KisNodeSP, QRect>& effectivePrevDirtyRects(int levelOfDetail) {
            return levelOfDetail > 0 ? prevDirtyPreviewRects : prevDirtyRects;
        }

        const KisSavedMacroCommand *overriddenCommand = 0;
        QVector<const KUndo2Command*> skippedWhileMergeCommands;

        bool updatesDisabled = false;
        bool finalizingActionsStarted = false;


        void executeAndAddCommand(KUndo2Command *cmd, KisStrokeStrategyUndoCommandBased *interface, CommandGroup group);

        void notifyAllCommandsDone(KisStrokeStrategyUndoCommandBased *interface);
        void undoAllCommands(KisStrokeStrategyUndoCommandBased *interface);
        void undoTransformCommands(KisStrokeStrategyUndoCommandBased *interface, int levelOfDetail);

        void postAllUpdates(int levelOfDetail);

        void transformNode(KisNodeSP node, const ToolTransformArgs &config, KisStrokeStrategyUndoCommandBased *interface, int levelOfDetail);
        void reapplyTransform(ToolTransformArgs args, QVector<KisStrokeJobData *> &mutatedJobs, KisStrokeStrategyUndoCommandBased *interface, int levelOfDetail);
        void finalizeStrokeImpl(QVector<KisStrokeJobData *> &mutatedJobs, KisStrokeStrategyUndoCommandBased *interface, bool saveCommands);

        void finishAction(QVector<KisStrokeJobData *> &mutatedJobs, KisStrokeStrategyUndoCommandBased *interface);
        void cancelAction(QVector<KisStrokeJobData *> &mutatedJobs, KisStrokeStrategyUndoCommandBased *interface);
        void addDirtyRect(KisNodeSP node, const QRect &rect, int levelOfDetail);

        bool postProcessToplevelCommand(KUndo2Command *command);
    };
    using SharedDataSP = QSharedPointer<SharedData>;

    Q_ENUM(CommandGroup);

public:
    InplaceTransformStrokeStrategy(ToolTransformArgs::TransformMode mode,
                                   bool workRecursively,
                                   const QString &filterId,
                                   bool forceReset,
                                   KisNodeSP rootNode,
                                   KisSelectionSP selection,
                                   KisStrokeUndoFacade *undoFacade,
                                   KisUpdatesFacade *updatesFacade, KisNodeSP imageRoot, bool forceLodMode);

    ~InplaceTransformStrokeStrategy() override;

    static bool shouldRestartStrokeOnModeChange(ToolTransformArgs::TransformMode oldMode,
                                                ToolTransformArgs::TransformMode newMode,
                                                KisNodeList processedNodes);

    void initStrokeCallback() override;
    void finishStrokeCallback() override;
    void cancelStrokeCallback() override;
    void doStrokeCallback(KisStrokeJobData *data) override;

    static bool fetchArgsFromCommand(const KUndo2Command *command, ToolTransformArgs *args, KisNodeSP *rootNode, KisNodeList *transformedNodes);

Q_SIGNALS:
    void sigTransactionGenerated(TransformTransactionProperties transaction, ToolTransformArgs args, void *cookie);

protected:
    void postProcessToplevelCommand(KUndo2Command *command) override;

private:
    friend class InitializeTransformModeStrokeStrategy;

    InplaceTransformStrokeStrategy(const InplaceTransformStrokeStrategy &rhs, int levelOfDetail);

    KoUpdaterPtr fetchUpdater(KisNodeSP node);


    void transformDevice(const ToolTransformArgs &config,
                         KisPaintDeviceSP device,
                         KisProcessingVisitor::ProgressHelper *helper);

    void clearSelection(KisPaintDeviceSP device);

    KisPaintDeviceSP createDeviceCache(KisPaintDeviceSP src);

    bool haveDeviceInCache(KisPaintDeviceSP src);
    void putDeviceCache(KisPaintDeviceSP src, KisPaintDeviceSP cache);
    KisPaintDeviceSP getDeviceCache(KisPaintDeviceSP src);

    static QList<KisNodeSP> fetchNodesList(ToolTransformArgs::TransformMode mode, KisNodeSP root, bool recursive);
    ToolTransformArgs resetArgsForMode(ToolTransformArgs::TransformMode mode,
                                       const QString &filterId,
                                       const TransformTransactionProperties &transaction);
    static KisNodeSP tryOverrideRootToTransformMask(KisNodeSP root);
    static bool tryInitArgsFromNode(KisNodeSP node, ToolTransformArgs *args);
    static bool tryFetchArgsFromCommandAndUndo(ToolTransformArgs *args,
                                               ToolTransformArgs::TransformMode mode,
                                               KisNodeSP currentNode,
                                               KisNodeList selectedNodes, KisStrokeUndoFacade *undoFacade,
                                               QVector<KisStrokeJobData *> *undoJobs,
                                               const KisSavedMacroCommand **overriddenCommand);

    void finishStrokeImpl(bool applyTransform,
                          const ToolTransformArgs &args);

    static void finalizeStrokeImpl(SharedDataSP m_s, QVector<KisStrokeJobData *> &mutatedJobs);

    void clearNode(KisNodeSP node);
    void transformNode(KisNodeSP node, const ToolTransformArgs &config);

    void tryPostUpdateJob(bool forceUpdate);
    void doCanvasUpdate(bool forceUpdate);

    int calculatePreferredLevelOfDetail(const QRect &srcRect);

public:
    static void transformAndMergeDevice(const ToolTransformArgs &config,
                                 KisPaintDeviceSP src,
                                 KisPaintDeviceSP dst,
                                 KisProcessingVisitor::ProgressHelper *helper);
private:
    SharedDataSP m_s;

    boost::optional<ToolTransformArgs> m_pendingUpdateArgs;
    QElapsedTimer m_updateTimer;
    const int m_updateInterval = 30;

    QVector<KisDecoratedNodeInterface*> m_disabledDecoratedNodes;
};

#endif /* __INPLACE_TRANSFORM_STROKE_STRATEGY_H */
