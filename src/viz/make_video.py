"""Event model for JesseSort card visualizations.

This module deliberately has no pygame dependency.  It models the core
bidirectional *physical pile* insertion used by the legacy physical JesseSort
pipeline: adjacent values choose the ascending or descending game, each game
binary-searches its contiguous pile-tail array, and the value is physically
appended to the selected pile.

The model is pedagogical: specialized production routes, prefix materializing,
and merging are intentionally omitted.  That keeps the event stream reusable
for visualization while preserving the actual pile-search inequalities.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Sequence

ASCENDING_GAME = 1
DESCENDING_GAME = 0
GAME_NAMES = {
    DESCENDING_GAME: "descending",
    ASCENDING_GAME: "ascending",
}


@dataclass(frozen=True)
class SearchProbe:
    """One binary-search comparison against a pile tail."""

    lo: int
    hi: int
    mid: int
    tail_value: int
    decision: str


@dataclass(frozen=True)
class PlacementStep:
    """Everything needed to animate one input value."""

    index: int
    value: int
    previous_value: int | None
    direction: str
    game: int
    pile_index: int
    new_pile: bool
    replaced_tail: int | None
    probes: tuple[SearchProbe, ...]
    tails_before: tuple[int, ...]
    tails_after: tuple[int, ...]
    piles_before: tuple[tuple[int, ...], ...]
    piles_after: tuple[tuple[int, ...], ...]
    repeat_same_pile: bool = False

    @property
    def game_name(self) -> str:
        return GAME_NAMES[self.game]


@dataclass(frozen=True)
class PhysicalPileTrace:
    values: tuple[int, ...]
    steps: tuple[PlacementStep, ...]
    final_piles_descending: tuple[tuple[int, ...], ...]
    final_piles_ascending: tuple[tuple[int, ...], ...]
    final_card_piles_descending: tuple[tuple[tuple[int, int], ...], ...]
    final_card_piles_ascending: tuple[tuple[tuple[int, int], ...], ...]


# Fixed 32-value mod20 stream chosen for visualization. Values are generated
# from deterministic seed 76, with deliberate equal-value transitions around
# two-thirds of the way through so the video can demonstrate repeat handling.
# This order yields exactly eight non-singleton physical piles, giving the
# merge visualization a clean 8 -> 4 -> 2 -> 1 progression.
DEFAULT_VALUES_32: tuple[int, ...] = (
    11, 14, 12, 6, 9, 1, 13, 7,
    9, 9, 14, 15, 7, 2, 10, 9,
    7, 10, 17, 1, 2, 2, 18, 17,
    17, 7, 10, 17, 14, 0, 2, 12,
)


def choose_game(values: Sequence[int], index: int, previous_game: int | None) -> tuple[int, str]:
    """Mirror the core physical pipeline's adj-direction game choice.

    For the first card, use the first real transition as the initial direction.
    On equivalent adjacent values the physical pipeline keeps the current game.
    The default visualization permutation is unique, so the tie rule is mostly
    here to keep custom inputs faithful.
    """

    value = values[index]
    if index == 0:
        # Pedagogical initialization: there is no previous card yet, so place
        # the first card directly into the ascending game. The second card is
        # the first true adj-direction comparison.
        return ASCENDING_GAME, "start"

    previous = values[index - 1]
    if value < previous:
        return DESCENDING_GAME, "down"
    if value > previous:
        return ASCENDING_GAME, "up"
    return (previous_game if previous_game is not None else ASCENDING_GAME), "equal"


def _search_descending_game(tails_ascending: Sequence[int], value: int) -> tuple[int, tuple[SearchProbe, ...]]:
    """Find first tail >= value.  Descending-game tails are ascending."""

    lo, hi = 0, len(tails_ascending)
    probes: list[SearchProbe] = []
    while lo < hi:
        mid = (lo + hi) // 2
        tail = tails_ascending[mid]
        if tail >= value:
            probes.append(SearchProbe(lo, hi, mid, tail, "tail >= card -> left"))
            hi = mid
        else:
            probes.append(SearchProbe(lo, hi, mid, tail, "tail < card -> right"))
            lo = mid + 1
    return lo, tuple(probes)


def _search_ascending_game(tails_descending: Sequence[int], value: int) -> tuple[int, tuple[SearchProbe, ...]]:
    """Find first tail <= value.  Ascending-game tails are descending."""

    lo, hi = 0, len(tails_descending)
    probes: list[SearchProbe] = []
    while lo < hi:
        mid = (lo + hi) // 2
        tail = tails_descending[mid]
        if tail <= value:
            probes.append(SearchProbe(lo, hi, mid, tail, "tail <= card -> left"))
            hi = mid
        else:
            probes.append(SearchProbe(lo, hi, mid, tail, "tail > card -> right"))
            lo = mid + 1
    return lo, tuple(probes)


def simulate_physical_piles(values: Iterable[int]) -> PhysicalPileTrace:
    values_tuple = tuple(int(v) for v in values)
    if not values_tuple:
        raise ValueError("physical-piles visualization requires at least one value")

    piles: dict[int, list[list[int]]] = {
        DESCENDING_GAME: [],
        ASCENDING_GAME: [],
    }
    card_piles: dict[int, list[list[tuple[int, int]]]] = {
        DESCENDING_GAME: [],
        ASCENDING_GAME: [],
    }
    tails: dict[int, list[int]] = {
        DESCENDING_GAME: [],
        ASCENDING_GAME: [],
    }
    steps: list[PlacementStep] = []
    previous_game: int | None = None
    previous_pile: int | None = None

    for index, value in enumerate(values_tuple):
        game, direction = choose_game(values_tuple, index, previous_game)
        game_tails = tails[game]
        game_piles = piles[game]

        tails_before = tuple(game_tails)
        piles_before = tuple(tuple(p) for p in game_piles)

        repeat_same_pile = direction == "equal" and previous_pile is not None and previous_game == game
        if repeat_same_pile:
            # Equal adjacent values stay in the previous game and on the exact
            # same pile. No pile-tail search is needed.
            pile_index = previous_pile
            probes = ()
            new_pile = False
            replaced_tail = game_tails[pile_index]
            game_tails[pile_index] = value
            game_piles[pile_index].append(value)
            card_piles[game][pile_index].append((value, index))
        else:
            if game == DESCENDING_GAME:
                pile_index, probes = _search_descending_game(game_tails, value)
            else:
                pile_index, probes = _search_ascending_game(game_tails, value)

            new_pile = pile_index == len(game_piles)
            replaced_tail = None if new_pile else game_tails[pile_index]
            if new_pile:
                game_tails.append(value)
                game_piles.append([value])
                card_piles[game].append([(value, index)])
            else:
                game_tails[pile_index] = value
                game_piles[pile_index].append(value)
                card_piles[game][pile_index].append((value, index))

        steps.append(
            PlacementStep(
                index=index,
                value=value,
                previous_value=None if index == 0 else values_tuple[index - 1],
                direction=direction,
                game=game,
                pile_index=pile_index,
                new_pile=new_pile,
                replaced_tail=replaced_tail,
                probes=probes,
                tails_before=tails_before,
                tails_after=tuple(game_tails),
                piles_before=piles_before,
                piles_after=tuple(tuple(p) for p in game_piles),
                repeat_same_pile=repeat_same_pile,
            )
        )
        previous_game = game
        previous_pile = pile_index

    trace = PhysicalPileTrace(
        values=values_tuple,
        steps=tuple(steps),
        final_piles_descending=tuple(tuple(p) for p in piles[DESCENDING_GAME]),
        final_piles_ascending=tuple(tuple(p) for p in piles[ASCENDING_GAME]),
        final_card_piles_descending=tuple(tuple(p) for p in card_piles[DESCENDING_GAME]),
        final_card_piles_ascending=tuple(tuple(p) for p in card_piles[ASCENDING_GAME]),
    )
    validate_trace(trace)
    return trace


def validate_trace(trace: PhysicalPileTrace) -> None:
    """Check pile monotonicity, assignment coverage, and tail-order invariants."""

    if len(trace.steps) != len(trace.values):
        raise AssertionError("one placement step is required per input value")

    seen: list[int] = []
    for piles_for_game, game in (
        (trace.final_piles_descending, DESCENDING_GAME),
        (trace.final_piles_ascending, ASCENDING_GAME),
    ):
        for pile in piles_for_game:
            if game == DESCENDING_GAME and any(a < b for a, b in zip(pile, pile[1:])):
                raise AssertionError(f"descending pile is not nonincreasing: {pile}")
            if game == ASCENDING_GAME and any(a > b for a, b in zip(pile, pile[1:])):
                raise AssertionError(f"ascending pile is not nondecreasing: {pile}")
            seen.extend(pile)

    if sorted(seen) != sorted(trace.values):
        raise AssertionError("final piles do not contain exactly the input multiset")

    desc_tails = [p[-1] for p in trace.final_piles_descending]
    asc_tails = [p[-1] for p in trace.final_piles_ascending]
    if desc_tails != sorted(desc_tails):
        raise AssertionError("descending-game pile tails must be ascending")
    if asc_tails != sorted(asc_tails, reverse=True):
        raise AssertionError("ascending-game pile tails must be descending")


def pile_to_ascending_run(pile: Sequence[int], game: int) -> tuple[int, ...]:
    """Read one physical pile in ascending order for merge visualization."""
    data = tuple(int(v) for v in pile)
    if game == DESCENDING_GAME:
        return tuple(reversed(data))
    if game == ASCENDING_GAME:
        return data
    raise ValueError(f"unknown game: {game}")


def merge_sorted_runs(left: Sequence[int], right: Sequence[int]) -> tuple[int, ...]:
    """Stable two-way merge used by the viz adj-pair merge phase."""
    i = j = 0
    out: list[int] = []
    while i < len(left) and j < len(right):
        if left[i] <= right[j]:
            out.append(int(left[i]))
            i += 1
        else:
            out.append(int(right[j]))
            j += 1
    out.extend(int(v) for v in left[i:])
    out.extend(int(v) for v in right[j:])
    return tuple(out)


def adjacent_bottom_up_merge_levels(runs: Sequence[Sequence[int]]) -> tuple[tuple[tuple[int, ...], ...], ...]:
    """Return run states for adjacent bottom-up pairwise merging.

    Level 0 is the input run list.  Each subsequent level merges adjacent
    pairs (0,1), (2,3), ... and carries an odd final run unchanged.
    """
    current = tuple(tuple(int(v) for v in run) for run in runs)
    if not current:
        return ((),)
    levels: list[tuple[tuple[int, ...], ...]] = [current]
    while len(current) > 1:
        nxt: list[tuple[int, ...]] = []
        for i in range(0, len(current), 2):
            if i + 1 < len(current):
                nxt.append(merge_sorted_runs(current[i], current[i + 1]))
            else:
                nxt.append(current[i])
        current = tuple(nxt)
        levels.append(current)
    return tuple(levels)




def pile_cards_to_ascending_run(pile: Sequence[tuple[int, int]], game: int) -> tuple[tuple[int, int], ...]:
    data = tuple((int(v), int(i)) for v, i in pile)
    return tuple(reversed(data)) if game == DESCENDING_GAME else data


def merge_sorted_card_runs(left: Sequence[tuple[int, int]], right: Sequence[tuple[int, int]]) -> tuple[tuple[int, int], ...]:
    i = j = 0
    out: list[tuple[int, int]] = []
    while i < len(left) and j < len(right):
        if left[i][0] <= right[j][0]:
            out.append(left[i]); i += 1
        else:
            out.append(right[j]); j += 1
    out.extend(left[i:]); out.extend(right[j:])
    return tuple(out)


def adjacent_bottom_up_card_merge_levels(runs: Sequence[Sequence[tuple[int, int]]]) -> tuple[tuple[tuple[tuple[int, int], ...], ...], ...]:
    current = tuple(tuple(run) for run in runs)
    if not current:
        return ((),)
    levels = [current]
    while len(current) > 1:
        nxt = []
        for i in range(0, len(current), 2):
            nxt.append(merge_sorted_card_runs(current[i], current[i + 1]) if i + 1 < len(current) else current[i])
        current = tuple(nxt)
        levels.append(current)
    return tuple(levels)

def global_merge_levels_for_physical_piles(trace: PhysicalPileTrace):
    """Return adjacent bottom-up merge levels using stable card identities."""
    runs = (
        tuple(pile_cards_to_ascending_run(p, DESCENDING_GAME) for p in trace.final_card_piles_descending)
        + tuple(pile_cards_to_ascending_run(p, ASCENDING_GAME) for p in trace.final_card_piles_ascending)
    )
    levels = adjacent_bottom_up_card_merge_levels(runs)
    if levels and levels[-1]:
        final_values = tuple(card[0] for card in levels[-1][0])
        if final_values != tuple(sorted(trace.values)):
            raise AssertionError("global merge levels did not produce sorted output")
    return levels

def merge_trace_for_physical_piles(trace: PhysicalPileTrace) -> dict[str, tuple[tuple[tuple[int, ...], ...], ...] | tuple[int, ...]]:
    """Build deterministic merge levels for both games and the final merge."""
    desc_runs = tuple(pile_to_ascending_run(p, DESCENDING_GAME) for p in trace.final_piles_descending)
    asc_runs = tuple(pile_to_ascending_run(p, ASCENDING_GAME) for p in trace.final_piles_ascending)
    desc_levels = adjacent_bottom_up_merge_levels(desc_runs)
    asc_levels = adjacent_bottom_up_merge_levels(asc_runs)
    desc_final = desc_levels[-1][0] if desc_levels and desc_levels[-1] else ()
    asc_final = asc_levels[-1][0] if asc_levels and asc_levels[-1] else ()
    final = merge_sorted_runs(desc_final, asc_final)
    return {
        "descending_levels": desc_levels,
        "ascending_levels": asc_levels,
        "descending_final": desc_final,
        "ascending_final": asc_final,
        "final": final,
    }


if __name__ == "__main__":
    trace = simulate_physical_piles(DEFAULT_VALUES_32)
    print("values:", len(trace.values))
    print("descending piles:", len(trace.final_piles_descending), trace.final_piles_descending)
    print("ascending piles:", len(trace.final_piles_ascending), trace.final_piles_ascending)
    print("search probes:", sum(len(step.probes) for step in trace.steps))


# -----------------------------------------------------------------------------
# Headless MP4 renderer
# -----------------------------------------------------------------------------
import argparse
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

from PIL import Image, ImageDraw, ImageFont


BG=(21,24,28); PANEL=(31,35,41); PANEL_2=(58,64,72); TEXT=(238,239,241); MUTED=(160,168,178)
CARD_FACE=(246,242,230); CARD_EDGE=(25,27,31); ASCENT=(68,120,93); DESCENT=(139,77,74)
PROBE=(221,174,77); CURRENT=(235,205,111); SHADOW=(12,14,17); MERGE=(91,116,151); EMPTY=(70,76,84); TEACH=(203,211,221)


@dataclass(frozen=True)
class Layout:
    width:int=1280; height:int=1080; margin:int=34
    input_y:int=84; input_card_w:int=32; input_card_h:int=44; input_gap:int=5
    work_y:int=142; work_h:int=106
    pile_card_w:int=66; pile_card_h:int=88; pile_step_y:int=31
    board_y:int=262; board_bottom:int=610
    merge_source_y:int=654; merge_tier_gap:int=82
    merge_card_w:int=26; merge_card_h:int=38; merge_card_gap:int=3; boundary_slot_w:int=18


class Fonts:
    def __init__(self):
        self.title=self._load(27,True); self.subtitle=self._load(17); self.body=self._load(16)
        self.small=self._load(13); self.tiny=self._load(11); self.teacher=self._load(31,True); self.teacher_small=self._load(24); self.teacher_note=self._load(17); self.legend=self._load(18,True); self.legend_sub=self._load(15); self.hero=self._load(52,True); self.hero_sub=self._load(23); self.card=self._load(24,True); self.corner=self._load(16,True)
    @staticmethod
    def _load(size,bold=False):
        name='DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf'
        for base in ('/usr/share/fonts/truetype/dejavu','/usr/share/fonts/dejavu'):
            p=Path(base)/name
            if p.exists(): return ImageFont.truetype(str(p),size=size)
        return ImageFont.load_default()


def text(draw,font,s,pos,color=TEXT): draw.text(pos,s,fill=color,font=font)
def rounded(draw,box,fill,outline=None,width=1,radius=8): draw.rounded_rectangle(box,radius=radius,fill=fill,outline=outline,width=width)

def translucent_panel(draw,box,fill=(246,242,230),alpha=204,outline=(35,38,42),width=2,radius=12):
    image=draw._image
    overlay=Image.new('RGBA',image.size,(0,0,0,0))
    od=ImageDraw.Draw(overlay)
    od.rounded_rectangle(box,radius=radius,fill=(*fill,alpha),outline=(*outline,255) if outline else None,width=width)
    if image.mode!='RGBA':
        base=image.convert('RGBA')
        base.alpha_composite(overlay)
        image.paste(base.convert(image.mode))
    else:
        image.alpha_composite(overlay)
def tsize(draw,s,font):
    b=draw.textbbox((0,0),s,font=font); return b[2]-b[0],b[3]-b[1]


def draw_card(draw,fonts,value,rect,*,game=None,active=False,dim=False,tiny=False,stacked=False):
    x,y,w,h=rect; r=max(4,w//9)
    rounded(draw,(x+3,y+4,x+w+3,y+h+4),SHADOW,radius=r)
    rounded(draw,(x,y,x+w,y+h),(180,179,173) if dim else CARD_FACE,CURRENT if active else CARD_EDGE,4 if active else 2,r)
    if game is not None:
        bh=max(4,h//12); rounded(draw,(x+2,y+h-bh-2,x+w-2,y+h-2),ASCENT if game==ASCENDING_GAME else DESCENT,radius=2)
    s=str(value)
    if tiny:
        tw,th=tsize(draw,s,fonts.small); draw.text((x+(w-tw)/2,y+(h-th)/2-1),s,fill=CARD_EDGE,font=fonts.small)
    elif stacked:
        draw.text((x+6,y+4),s,fill=CARD_EDGE,font=fonts.corner)
    else:
        tw,th=tsize(draw,s,fonts.card); draw.text((x+(w-tw)/2,y+(h-th)/2+3),s,fill=CARD_EDGE,font=fonts.card)


def draw_empty_slot(draw,rect):
    x,y,w,h=rect
    rounded(draw,(x,y,x+w,y+h),None,EMPTY,1,max(3,w//8))


def input_rect(L,i,n):
    total=n*L.input_card_w+(n-1)*L.input_gap; x0=(L.width-total)//2
    return (x0+i*(L.input_card_w+L.input_gap),L.input_y,L.input_card_w,L.input_card_h)


def panel_rect(L,game):
    gap=24; w=(L.width-2*L.margin-gap)//2; x=L.margin if game==DESCENDING_GAME else L.margin+w+gap
    return (x,L.board_y,w,L.board_bottom-L.board_y)


def pile_rect(L,game,pi,depth,count):
    px,py,pw,_=panel_rect(L,game); usable=pw-32
    if count<=1: gap=0; x0=px+pw//2-L.pile_card_w//2
    else:
        gap=min(16,max(3,(usable-count*L.pile_card_w)//(count-1))); total=count*L.pile_card_w+(count-1)*gap; x0=px+pw//2-total//2
    return (x0+pi*(L.pile_card_w+gap),py+66+depth*L.pile_step_y,L.pile_card_w,L.pile_card_h)


def interp(a,b,t):
    q=1-(1-max(0,min(1,t)))**3
    return tuple(round(x+(y-x)*q) for x,y in zip(a,b))


def draw_work(draw,F,L,step:PlacementStep|None,probe:SearchProbe|None,stage:str,piles_desc=(),piles_asc=()):
    x,y,w,h=L.margin,L.work_y,L.width-2*L.margin,L.work_h
    rounded(draw,(x,y,x+w,y+h),PANEL,PANEL_2,2,10); text(draw,F.small,'WORK AREA',(x+12,y+8),MUTED)

    # Left side: adj-card direction / game choice.
    if step is None:
        text(draw,F.body,"Compare to previous → choose a game → find the right pile.",(x+120,y+43),MUTED)
    else:
        bx=x+110; cy=y+60
        if step.previous_value is None:
            empty=(bx,cy-29,44,58); draw_empty_slot(draw,empty)
            text(draw,F.tiny,'none',(bx+9,cy-5),MUTED)
            draw_card(draw,F,step.value,(bx+90,cy-29,44,58),tiny=True,active=stage=='route')
            text(draw,F.tiny,'previous',(bx-3,y+84),MUTED); text(draw,F.tiny,'current',(bx+91,y+84),MUTED)
            text(draw,F.body,'first card → ascending game',(bx+160,cy-10),ASCENT)
        else:
            prev=step.previous_value
            rel='<' if step.value<prev else '>' if step.value>prev else '='
            draw_card(draw,F,prev,(bx,cy-29,44,58),tiny=True)
            text(draw,F.body,rel,(bx+61,cy-10),CURRENT)
            draw_card(draw,F,step.value,(bx+90,cy-29,44,58),tiny=True,active=stage=='route')
            text(draw,F.tiny,'previous',(bx-3,y+84),MUTED); text(draw,F.tiny,'current',(bx+91,y+84),MUTED)
            gc=ASCENT if step.game==ASCENDING_GAME else DESCENT
            text(draw,F.body,'→ ascending game' if step.game==ASCENDING_GAME else '→ descending game',(bx+160,cy-10),gc)

    # Right side: persistent copies of BOTH games' pile tails.  The inactive
    # game remains visible when routing switches, which makes the two evolving
    # tail arrays easier to track.  Only the active row gets binary-search
    # interval/probe emphasis.
    sx=x+575
    rows=(
        (DESCENDING_GAME,'descending tails',DESCENT,tuple(p[-1][0] if isinstance(p[-1],tuple) else p[-1] for p in piles_desc) if piles_desc else ()),
        (ASCENDING_GAME,'ascending tails',ASCENT,tuple(p[-1][0] if isinstance(p[-1],tuple) else p[-1] for p in piles_asc) if piles_asc else ()),
    )
    tw,th,gap=34,32,6
    for row_i,(game,label,color,current_tails) in enumerate(rows):
        ry=y+14+row_i*46
        active_game=step is not None and step.game==game
        # During search, use the exact pre-placement tail snapshot from the
        # event trace for the active game; otherwise the current physical-pile
        # tails are equivalent and stay persistent.
        tails=step.tails_before if active_game else current_tails
        text(draw,F.tiny,label,(sx,ry),color if active_game else MUTED)
        if not tails:
            text(draw,F.tiny,'— none yet —',(sx+112,ry+12),color if active_game else MUTED)
            continue
        for i,tail in enumerate(tails):
            r=(sx+112+i*(tw+gap),ry-4,tw,th)
            active=active_game and probe is not None and i==probe.mid
            in_range=(not active_game) or probe is None or probe.lo<=i<probe.hi
            draw_card(draw,F,tail,r,tiny=True,active=active,dim=not in_range)
        if active_game:
            if probe is not None:
                text(draw,F.tiny,f'search [{probe.lo},{probe.hi}) → check tail {probe.mid}',(sx+112,ry+29),PROBE)
            elif stage=='search':
                text(draw,F.tiny,'find the right pile  (binary search)',(sx+112,ry+29),PROBE)


def draw_pile_panels(draw,F,L,piles_desc,piles_asc,*,step=None,probe=None):
    for game,label,color in ((DESCENDING_GAME,'DESCENDING GAME',DESCENT),(ASCENDING_GAME,'ASCENDING GAME',ASCENT)):
        px,py,pw,ph=panel_rect(L,game); rounded(draw,(px,py,px+pw,py+ph),PANEL,PANEL_2,2,12)
        draw.ellipse((px+14,py+16,px+26,py+28),fill=color); text(draw,F.body,label,(px+34,py+10)); text(draw,F.small,'first tail >= card' if game==DESCENDING_GAME else 'first tail <= card',(px+34,py+34),MUTED)
        piles=piles_desc if game==DESCENDING_GAME else piles_asc
        for pi,pile in enumerate(piles):
            for depth,item in enumerate(pile):
                v=item[0] if isinstance(item,tuple) else item
                r=pile_rect(L,game,pi,depth,len(piles))
                # Keep physical piles stable during tail search. Only the
                # persistent tail copies in the work area show probe emphasis.
                draw_card(draw,F,v,r,game=game,active=False,stacked=depth<len(pile)-1)


def arrow(draw,start,end,color=TEACH,width=4):
    draw.line((start,end),fill=color,width=width)
    import math
    ang=math.atan2(end[1]-start[1],end[0]-start[0]); size=12
    pts=[]
    for off in (2.55,-2.55):
        a=ang+off; pts.append((end[0]+size*math.cos(a),end[1]+size*math.sin(a)))
    draw.polygon([end,pts[0],pts[1]],fill=color)


def draw_intro_title(draw,F,L,processed:int,step:PlacementStep|None):
    # Elements 1–5: title only. Elements 6–10: title + subtitle.
    visible=(step is not None and step.index<10) or (step is None and processed<10)
    if not visible:
        return
    x,y,w,h=L.width//2-360,330,720,190
    translucent_panel(draw,(x,y,x+w,y+h),radius=14)
    tw,th=tsize(draw,'Jessesort',F.hero)
    draw.text((x+(w-tw)/2,y+34),'Jessesort',fill=CARD_EDGE,font=F.hero)
    if processed>=5:
        sub='Play Patience (similar to Solitaire) in both directions.'
        sw,sh=tsize(draw,sub,F.hero_sub)
        draw.text((x+(w-sw)/2,y+112),sub,fill=CARD_EDGE,font=F.hero_sub)


def draw_game_intro(draw,F,L,step:PlacementStep|None):
    # Elements 14–18: introduce both independent Patience games.
    if step is None or step.index<13 or step.index>=18:
        return
    y=690
    boxes=[(110,y,470,112,'One game with descending piles',DESCENT,DESCENDING_GAME),
           (700,y,470,112,'One game with ascending piles',ASCENT,ASCENDING_GAME)]
    for x,by,w,h,label,color,game in boxes:
        translucent_panel(draw,(x,by,x+w,by+h),radius=12)
        tw,th=tsize(draw,label,F.teacher_small)
        draw.text((x+(w-tw)/2,by+(h-th)/2),label,fill=CARD_EDGE,font=F.teacher_small)
        px,py,pw,ph=panel_rect(L,game)
        arrow(draw,(x+w//2,by),(px+pw//2,py+ph-18),color)


def teacher_copy(step:PlacementStep,probe:SearchProbe|None,stage:str):
    n=step.index+1
    direction='ascending' if step.game==ASCENDING_GAME else 'descending'
    if stage=='route':
        if step.repeat_same_pile:
            return (
                f'REPEAT VALUE  (CARD {n})',
                f'{step.previous_value} → {step.value}. Same value.',
                f'Stay in the {direction} game. Use the same pile.'
            )
        moved='up' if step.value>step.previous_value else 'down'
        return (
            f'STEP 1 — PICK A GAME  (CARD {n})',
            f'{step.previous_value} → {step.value}. The value went {moved}.',
            f'{direction.title()} direction → {direction} game.'
        )
    if stage=='search':
        if step.repeat_same_pile:
            return (
                f'NO SEARCH NEEDED  (CARD {n})',
                'Same value as the last card.',
                'Use the same game and same pile.'
            )
        if not step.tails_before:
            return (
                f'STEP 2 — FIND THE RIGHT PILE  (CARD {n})',
                'Look at this game’s pile tails.',
                'No piles yet. Start the first pile.'
            )
        if probe is None:
            return (
                f'STEP 2 — FIND THE RIGHT PILE  (CARD {n})',
                'Here’s a copy of this game’s pile tails.',
                'Search the tails to find the right pile.'
            )
        if step.game==ASCENDING_GAME:
            works = probe.tail_value <= step.value
        else:
            works = probe.tail_value >= step.value
        return (
            f'STEP 2 — FIND THE RIGHT PILE  (CARD {n})',
            f'Check tail {probe.tail_value}. ' + ('This pile works.' if works else 'Not this pile.'),
            'Keep looking until we find the right pile.'
        )
    if step.repeat_same_pile:
        return (
            f'REPEAT — SAME PILE  (CARD {n})',
            f'No search. Stay on pile {step.pile_index + 1}.',
            f'Put the second {step.value} on the same pile.'
        )
    return (
        f'STEP 3 — PLACE THE CARD  (CARD {n})',
        f'Pile {step.pile_index + 1} is the right pile.',
        f'Put card {step.value} there.'
    )


def draw_teacher(draw,F,L,step:PlacementStep|None,probe:SearchProbe|None,stage:str):
    if step is None or step.index not in TEACH_INDICES:
        return
    # Teaching area: left explanation window (80% of the former width) +
    # persistent three-step legend on the right. Both use 80% opacity.
    x,y,total_w,h=85,690,L.width-170,260
    gap=18
    left_w=round(total_w*0.80)
    right_x=x+left_w+gap
    right_w=x+total_w-right_x
    translucent_panel(draw,(x,y,x+left_w,y+h),radius=14)
    translucent_panel(draw,(right_x,y,right_x+right_w,y+h),radius=14)

    title,line1,line2=teacher_copy(step,probe,stage)
    text(draw,F.teacher,title,(x+28,y+24),CARD_EDGE)
    text(draw,F.teacher_small,line1,(x+28,y+86),CARD_EDGE)
    text(draw,F.teacher_small,line2,(x+28,y+130),CARD_EDGE)
    if stage=='search': text(draw,F.teacher_note,'(binary search)',(x+28,y+170),MUTED)

    active_step=1 if stage=='route' else 2 if stage=='search' else 3
    text(draw,F.legend,'THREE STEPS',(right_x+18,y+16),CARD_EDGE)
    legend=[
        ('1', 'Pick a game', 'Compare to previous'),
        ('2', 'Find a pile', 'Look at pile tails'),
        ('3', 'Place card', 'Put it on that pile'),
    ]
    ly=y+55
    for number,line_a,line_b in legend:
        n=int(number)
        row=(right_x+12,ly-5,right_x+right_w-12,ly+52)
        if n==active_step:
            # Subtle highlight inside the translucent legend.
            rounded(draw,row,(235,205,111),CARD_EDGE,1,8)
            color=CARD_EDGE
        else:
            color=(80,82,86)
        text(draw,F.legend,f'{number}. {line_a}',(right_x+20,ly),color)
        text(draw,F.legend_sub,line_b,(right_x+25,ly+28),color)
        ly+=63

    # Stage-specific arrows keep the explanation focused while leaving the
    # right-side legend stationary.
    if stage=='route':
        bx=L.margin+110; cy=L.work_y+60
        arrow(draw,(x+220,y),(bx+68,cy+30),CURRENT)
        text(draw,F.teacher_note,'compare these two cards',(x+38,y+198),CURRENT)
        px,py,pw,ph=panel_rect(L,step.game)
        gc=ASCENT if step.game==ASCENDING_GAME else DESCENT
        arrow(draw,(x+left_w-180,y),(px+110,py+28),gc)
        text(draw,F.teacher_note,f'send it to the {step.game_name} game',(x+left_w-330,y+198),gc)
    elif stage=='search':
        tail_row_y=L.work_y+(14 if step.game==DESCENDING_GAME else 60)
        tail_target=(L.margin+575+190,tail_row_y+12)
        arrow(draw,(x+left_w//2,y),tail_target,PROBE)
        text(draw,F.teacher_note,"copy of this game's pile tails",(x+left_w//2-135,y+198),PROBE)
    else:
        px,py,pw,ph=panel_rect(L,step.game)
        pile_count=max(1,len(step.piles_after))
        depth=max(0,len(step.piles_after[step.pile_index])-1)
        tr=pile_rect(L,step.game,step.pile_index,depth,pile_count)
        gc=ASCENT if step.game==ASCENDING_GAME else DESCENT
        arrow(draw,(x+left_w//2,y),(tr[0]+tr[2]//2,tr[1]+30),gc)
        text(draw,F.teacher_note,f'place card {step.value} on the chosen pile',(x+left_w//2-145,y+198),gc)

def insertion_frame(L,F,values,processed,piles_desc,piles_asc,*,step=None,probe=None,moving=None,status='',stage='route'):
    im=Image.new('RGB',(L.width,L.height),BG); d=ImageDraw.Draw(im)
    text(d,F.title,'JesseSort physical piles',(L.margin,18)); text(d,F.subtitle,'dual-direction Patience insertion with real cards',(L.margin,49),MUTED)
    text(d,F.small,'INPUT STREAM',(L.margin,L.input_y-20),MUTED)
    for i,v in enumerate(values): draw_card(d,F,v,input_rect(L,i,len(values)),dim=i<processed,active=step is not None and i==step.index,tiny=True)
    draw_work(d,F,L,step,probe,stage,piles_desc,piles_asc)
    draw_pile_panels(d,F,L,piles_desc,piles_asc,step=step,probe=probe)
    draw_game_intro(d,F,L,step)
    draw_teacher(d,F,L,step,probe,stage)
    if status: text(d,F.tiny,status,(L.margin+8,L.board_bottom+10),MUTED)
    if moving is not None: draw_card(d,F,moving[0],moving[1],game=step.game if step else None,active=True)
    draw_intro_title(d,F,L,processed,step)
    return im


def card_key(card):
    return tuple(card) if isinstance(card, (tuple, list)) and len(card) == 2 else (int(card), int(card))

def card_value(card):
    return int(card[0]) if isinstance(card, (tuple, list)) else int(card)

TEACH_INDICES = {20: 'ascending', 22: 'descending', 24: 'repeat'}

def row_layout(L,runs,y):
    """Return card positions and blank boundary slots for one horizontal run tier."""
    card_count=sum(len(r) for r in runs); boundaries=max(0,len(runs)-1)
    total=card_count*(L.merge_card_w+L.merge_card_gap)-L.merge_card_gap + boundaries*L.boundary_slot_w
    x=(L.width-total)//2
    pos={}; blanks=[]
    for ri,run in enumerate(runs):
        for card in run:
            pos[card_key(card)]=(x,y,L.merge_card_w,L.merge_card_h)
            x += L.merge_card_w+L.merge_card_gap
        if ri+1<len(runs):
            x -= L.merge_card_gap
            blanks.append((x,y,L.boundary_slot_w,L.merge_card_h))
            x += L.boundary_slot_w+L.merge_card_gap
    return pos,blanks




def run_center_from_layout(L, runs, y, run_index):
    pos,_=row_layout(L,runs,y)
    run=runs[run_index]
    if not run:
        return (L.width//2, y + L.merge_card_h//2)
    first=pos[card_key(run[0])]
    last=pos[card_key(run[-1])]
    left=first[0]
    right=last[0]+last[2]
    return ((left+right)//2, y + L.merge_card_h//2)


def draw_merge_connectors(draw,L,source_runs,target_runs,source_y,target_y,*,progress=1.0):
    """Draw V-shaped donor connectors for adj-pair bottom-up merges."""
    q=max(0.0,min(1.0,float(progress)))
    if q<=0:
        return
    color=(105,128,158)
    for target_index in range(len(target_runs)):
        left_index=2*target_index
        right_index=left_index+1
        if right_index>=len(source_runs):
            continue  # odd carry-forward run: no merge V
        lx,ly=run_center_from_layout(L,source_runs,source_y,left_index)
        rx,ry=run_center_from_layout(L,source_runs,source_y,right_index)
        tx,ty=run_center_from_layout(L,target_runs,target_y,target_index)
        start_y=source_y+L.merge_card_h+5
        end_y=target_y-7
        # Animate the line endpoints downward during the tier transition.
        ey=round(start_y+(end_y-start_y)*q)
        exl=round(lx+(tx-lx)*q)
        exr=round(rx+(tx-rx)*q)
        draw.line((lx,start_y,exl,ey),fill=color,width=3)
        draw.line((rx,start_y,exr,ey),fill=color,width=3)

def pile_value_positions(L,piles_desc,piles_asc):
    out={}
    for game,piles in ((DESCENDING_GAME,piles_desc),(ASCENDING_GAME,piles_asc)):
        for pi,pile in enumerate(piles):
            for depth,item in enumerate(pile):
                key=(int(item[0]),int(item[1])) if isinstance(item,tuple) else (int(item),-1)
                out[key]=pile_rect(L,game,pi,depth,len(piles))
    return out


def draw_merge_base(L,F,piles_desc,piles_asc,levels,shown_rows,label):
    im=Image.new('RGB',(L.width,L.height),BG); d=ImageDraw.Draw(im)
    text(d,F.title,'JesseSort merge phase',(L.margin,18)); text(d,F.subtitle,'adjacent bottom-up pairwise merging',(L.margin,49),MUTED)
    draw_pile_panels(d,F,L,piles_desc,piles_asc)
    text(d,F.body,label,(L.margin,L.board_bottom+8),MERGE)

    row_names=['PILE RUNS','MERGE TIER 1','MERGE TIER 2','MERGE TIER 3','MERGE TIER 4']
    for row_idx in range(shown_rows+1):
        y=L.merge_source_y + row_idx*L.merge_tier_gap
        runs=levels[row_idx]
        if row_idx>0:
            source_y=L.merge_source_y+(row_idx-1)*L.merge_tier_gap
            draw_merge_connectors(d,L,levels[row_idx-1],runs,source_y,y)
        text(d,F.tiny,row_names[row_idx],(L.margin,y+10),MUTED if row_idx==0 else MERGE)
        pos,blanks=row_layout(L,runs,y)
        for b in blanks: draw_empty_slot(d,b)
        for run in runs:
            for card in run:
                draw_card(d,F,card_value(card),pos[card_key(card)],tiny=True)
    return im


def merge_transition_frame(L,F,piles_desc,piles_asc,levels,from_idx,to_idx,t,label):
    # Keep all completed rows through from_idx, then animate copies downward.
    im=draw_merge_base(L,F,piles_desc,piles_asc,levels,from_idx,label)
    d=ImageDraw.Draw(im)
    sy=L.merge_source_y+from_idx*L.merge_tier_gap; ty=L.merge_source_y+to_idx*L.merge_tier_gap
    start,_=row_layout(L,levels[from_idx],sy); target,_=row_layout(L,levels[to_idx],ty)
    q=1-(1-max(0,min(1,t)))**3
    draw_merge_connectors(d,L,levels[from_idx],levels[to_idx],sy,ty,progress=q)
    # Draw target blank boundary slots gradually as the tier forms.
    _,blanks=row_layout(L,levels[to_idx],ty)
    if q>0.55:
        for b in blanks: draw_empty_slot(d,b)
    for key,sr in start.items():
        rr=interp(sr,target[key],q); draw_card(d,F,key[0],rr,tiny=True,active=q<0.98)
    return im


def source_transition_frame(L,F,piles_desc,piles_asc,levels,t,label):
    # Animate cards from their actual physical-pile locations into the source strip.
    im=Image.new('RGB',(L.width,L.height),BG); d=ImageDraw.Draw(im)
    text(d,F.title,'JesseSort merge phase',(L.margin,18)); text(d,F.subtitle,'materialize pile runs, then merge adjacent pairs',(L.margin,49),MUTED)
    draw_pile_panels(d,F,L,piles_desc,piles_asc)
    text(d,F.body,label,(L.margin,L.board_bottom+8),MERGE)
    ty=L.merge_source_y; target,blanks=row_layout(L,levels[0],ty); start=pile_value_positions(L,piles_desc,piles_asc)
    q=1-(1-max(0,min(1,t)))**3
    if q>0.55:
        for b in blanks: draw_empty_slot(d,b)
    for key,sr in start.items(): draw_card(d,F,key[0],interp(sr,target[key],q),tiny=True,active=q<0.98)
    return im


def parse_args():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--fps',type=int,default=30); p.add_argument('--speed',type=float,default=1.5); p.add_argument('--max-cards',type=int,default=32); p.add_argument('--output',type=Path,default=Path('src/viz/output/physical_piles_32.mp4')); p.add_argument('--no-comparisons',action='store_true'); return p.parse_args()


def main():
    a=parse_args(); values=DEFAULT_VALUES_32[:max(1,min(32,a.max_cards))]; trace=simulate_physical_piles(values); levels=global_merge_levels_for_physical_piles(trace)
    if len(values)==32 and [len(x) for x in levels] != [8,4,2,1]: raise AssertionError('fixed mod20 demo must produce 8→4→2→1 merge geometry')
    assert tuple(card_value(c) for c in levels[-1][0])==tuple(sorted(values))
    ffmpeg=shutil.which('ffmpeg')
    if not ffmpeg: raise SystemExit('ffmpeg required')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    cmd=[ffmpeg,'-y','-f','rawvideo','-pix_fmt','rgb24','-s',f'{Layout.width}x{Layout.height}','-r',str(a.fps),'-i','-','-an','-c:v','libx264','-preset','veryfast','-pix_fmt','yuv420p','-movflags','+faststart',str(a.output)]
    proc=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    L=Layout(); F=Fonts(); piles={DESCENDING_GAME:[],ASCENDING_GAME:[]}; count=0
    def emit(im):
        nonlocal count
        proc.stdin.write(im.tobytes()); count+=1
    def hold(seconds,fn,*,scaled=True,**kw):
        dur=seconds/a.speed if scaled else seconds
        im=fn(**kw)
        for _ in range(max(1,round(dur*a.fps))): emit(im)

    for step in trace.steps:
        teach=step.index in TEACH_INDICES
        # Step 1 and the copied-tail explanation get extra reading time.
        rh=4.0 if teach else 0.16; ph=3.0 if teach else 0.12; plh=3.0 if teach else 0.12
        processed=sum(len(p) for g in piles.values() for p in g)
        if step.previous_value is None:
            route_status=f'first card {step.value}: no previous card → initialize ascending game'
        else:
            route_status=f'natural direction: {step.previous_value} → {step.value}; choose {step.game_name} game'
        hold(rh,insertion_frame,L=L,F=F,values=values,processed=processed,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],step=step,stage='route',status=route_status,scaled=not teach)
        if teach and not step.repeat_same_pile:
            hold(4.0,insertion_frame,L=L,F=F,values=values,processed=processed,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],step=step,stage='search',status=f"look at {step.game_name} pile tails and find the right pile",scaled=False)
        if not a.no_comparisons:
            for probe in step.probes:
                hold(ph,insertion_frame,L=L,F=F,values=values,processed=processed,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],step=step,probe=probe,stage='search',status=f'find the right {step.game_name} pile; check pile {probe.mid + 1}',scaled=not teach)
        gp=piles[step.game]; tc=len(gp)+(1 if step.new_pile else 0); depth=0 if step.new_pile else len(gp[step.pile_index]); target=pile_rect(L,step.game,step.pile_index,depth,max(1,tc))
        sx,sy,sw,sh=input_rect(L,step.index,len(values)); start=(sx-16,sy-22,sw+32,sh+44); mf=max(2,round(((0.70 if teach else 0.253)/a.speed)*a.fps))
        for k in range(mf):
            mr=interp(start,target,(k+1)/mf)
            move_stage='place' if step.repeat_same_pile else 'search'
            move_status='same value → same pile' if step.repeat_same_pile else ('create new pile' if step.new_pile else f'place on pile {step.pile_index}')
            emit(insertion_frame(L,F,values,processed,piles[DESCENDING_GAME],piles[ASCENDING_GAME],step=step,moving=(step.value,mr),stage=move_stage,status=move_status))
        if step.new_pile: gp.append([])
        gp[step.pile_index].append((step.value,step.index)); processed+=1
        hold(plh,insertion_frame,L=L,F=F,values=values,processed=processed,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],step=step,stage='place',status=f'placed {step.value} on {step.game_name} pile {step.pile_index}',scaled=not teach)

    processed=len(values)
    # Exact requested pause between insertion and merge.
    hold(2.0,insertion_frame,L=L,F=F,values=values,processed=processed,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],status='Insertion complete. Pause before bottom-up adjacent merging.',scaled=False)

    # Materialize physical piles into the source strip.
    mf=max(2,round((0.75/a.speed)*a.fps))
    for k in range(mf): emit(source_transition_frame(L,F,piles[DESCENDING_GAME],piles[ASCENDING_GAME],levels,(k+1)/mf,'Read each physical pile as one ascending chunk.'))
    hold(0.55,draw_merge_base,L=L,F=F,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],levels=levels,shown_rows=0,label=f'{len(levels[0])} initial chunks; empty slots mark pile boundaries.')

    # Clean bottom-up merge tiers: 8→4→2→1.
    for tier in range(1,len(levels)):
        mf=max(2,round((0.85/a.speed)*a.fps))
        for k in range(mf): emit(merge_transition_frame(L,F,piles[DESCENDING_GAME],piles[ASCENDING_GAME],levels,tier-1,tier,(k+1)/mf,f'Merge tier {tier}: adjacent chunks merge; boundaries disappear.'))
        hold(0.55,draw_merge_base,L=L,F=F,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],levels=levels,shown_rows=tier,label=f'Merge tier {tier} complete: {len(levels[tier])} chunk' + ('s.' if len(levels[tier])!=1 else '.'))

    # Exact five-second final hold on all four tiers with one final sorted chunk.
    hold(5.0,draw_merge_base,L=L,F=F,piles_desc=piles[DESCENDING_GAME],piles_asc=piles[ASCENDING_GAME],levels=levels,shown_rows=len(levels)-1,label='Sorted output: one 32-card run.',scaled=False)

    proc.stdin.close(); rc=proc.wait()
    if rc: raise SystemExit(rc)
    print(f'wrote {a.output} ({count} frames, {count/a.fps:.2f}s)')
    return 0


if __name__=='__main__': raise SystemExit(main())
