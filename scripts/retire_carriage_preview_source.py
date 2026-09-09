"""One-shot removal of the user-retired, dedicated carriage preview implementation."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
def edit(path, fn):
    p=ROOT/path
    s=p.read_text(encoding='utf-8-sig')
    p.write_text(fn(s),encoding='utf-8')

def remove_block(s, marker):
    start=s.index(marker)
    opening=s.index('{',start)
    depth=1; end=opening+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    return s[:start]+s[end:]

def controller(s):
    for name in ['CarriageRig','AftermathController']:
        s=s.replace(f'#include "Town/GameXXKPrologue{name}.h"\n','')
    for marker in ['\tif (AGameXXKPrologueCarriageRig* Rig = ActivePrologueCarriageRig.Get())',
                   '\tif (AGameXXKPrologueAftermathController* Controller =\n\t\tActivePrologueAftermathController.Get())']:
        while marker in s:s=remove_block(s,marker)
    for ret,name in [('bool','BeginPrologueCarriagePresentation'),('void','EndPrologueCarriagePresentation'),
                     ('bool','SetPrologueCarriagePaused'),('bool','BeginPrologueAftermathPresentation'),
                     ('void','EndPrologueAftermathPresentation'),('bool','SetPrologueAftermathPaused'),
                     ('bool','RequestDesktopReturnFromPrologue'),('bool','RequestDesktopStoryCarriageFromWorkbench')]:
        s=remove_block(s,f'{ret} AGameXXKMVPPlayerController::{name}(')
    s=remove_block(s,'\tif (GameXXKLevelFlow::ShouldCollapseBackpackForTravelOptions(Options)')
    return s
edit('Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp',controller)
def header(s):
    s=re.sub(r'^class AGameXXKPrologue(?:CarriageRig|AftermathController);\n','',s,flags=re.M)
    start=s.index('\tbool BeginPrologueCarriagePresentation(');end=s.index('\tbool OpenTutorialMapInspection()',start)
    s=s[:start]+s[end:]
    start=s.index('\tUFUNCTION(BlueprintPure, Category = "GameXXK|Prologue|Carriage|Observation")')
    end=s.index('\n\t/** Single canonical battle-board',start)
    s=s[:start]+s[end:]
    s=s.replace('\tbool RequestDesktopStoryCarriageFromWorkbench();\n','')
    s=re.sub(r'^\t(?:TWeakObjectPtr<AGameXXKPrologue(?:CarriageRig|AftermathController)>.*|TWeakObjectPtr<AActor> ProloguePreviousViewTarget;|EGameXXKTrackedInputMode (?:Prologue|Aftermath)PreviousInputMode.*|bool b(?:Prologue|Aftermath)(?:Previous|Owned).*);?\n','',s,flags=re.M)
    return s
edit('Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h',header)
def flow(s):
    for ret,name in [('FString','CarriagePreviewTravelOptions'),('bool','HasCarriagePreviewTravelOption'),('bool','ShouldCollapseBackpackForTravelOptions')]:
        s=remove_block(s,f'{ret} GameXXKLevelFlow::{name}(')
    return s
edit('Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp',flow)
edit('Source/GameXXK/Public/MVP/GameXXKLevelFlow.h',lambda s:re.sub(r'\t/\*\*[^\n]*(?:carriage|backpack)[^\n]*\*/\n(?=\tGAMEXXK_API (?:FString Carriage|bool HasCarriage|bool ShouldCollapse))','',re.sub(r'^\tGAMEXXK_API (?:FString CarriagePreviewTravelOptions|bool HasCarriagePreviewTravelOption|bool ShouldCollapseBackpackForTravelOptions)\([^\n]*\);\n','',s,flags=re.M)))
edit('Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp',lambda s:remove_block(s,'bool UGameXXKDesktopTrainingWorkbenchWidget::RequestStoryCarriage('))
def workbench_header(s):
    s=re.sub(r'^DECLARE_DELEGATE_RetVal\(bool, FGameXXKStoryCarriageRequested\);\n','',s,flags=re.M)
    s=remove_block(s,'\tvoid SetStoryCarriageRequestedForTest(')
    s=s.replace('\tbool RequestStoryCarriage();\n','').replace('\tFGameXXKStoryCarriageRequested StoryCarriageRequested;\n','')
    return s
edit('Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h',workbench_header)
def level_tests(s):
    s=re.sub(r'\tTest(?:Equal|True|False)\(\s*TEXT\("(?:carriage[^"\n]*|ordinary town travel is not a carriage preview|ordinary town travel preserves its existing session policy)"\),.*?;\n','',s,flags=re.S)
    return s
edit('Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp',level_tests)
def widget_tests(s):
    start=s.index('IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n\tFGameXXKDesktopTrainingStoryQuestCarriageRequestTest,')
    end=s.find('IMPLEMENT_SIMPLE_AUTOMATION_TEST(',start+1)
    if end<0:raise RuntimeError('Test boundary missing')
    return s[:start]+s[end:]
edit('Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp',widget_tests)
removed=[]
for directory in ['Source/GameXXK/Public/Prologue','Source/GameXXK/Public/Town','Source/GameXXK/Public/UI',
                  'Source/GameXXK/Private/Prologue','Source/GameXXK/Private/Town','Source/GameXXK/Private/UI','Source/GameXXK/Private/Tests']:
    for p in (ROOT/directory).glob('GameXXKPrologue*'):
        if ('Carriage' in p.name or 'Aftermath' in p.name) and p.suffix in {'.h','.cpp'}:
            assert p.resolve().is_relative_to(ROOT.resolve())
            removed.append(str(p.relative_to(ROOT)));p.unlink()
print('\n'.join(removed))
