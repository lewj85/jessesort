import os
os.environ.setdefault('SDL_VIDEODRIVER','dummy')
os.environ.setdefault('SDL_AUDIODRIVER','dummy')
import math, random, colorsys, wave, struct, subprocess, shutil
from pathlib import Path
import pygame

# ================= CONFIG =================
NUM_INPUT_VALUES = 32
VISUAL_HEIGHT_BOOST = 8
RANDOM_SEED = 41
FREEZE_ALPHA = 0.368
FREEZE_POINT = round(NUM_INPUT_VALUES * FREEZE_ALPHA)
FRAMES_PER_STEP = 5
SORT_FRAMES_PER_EVENT = 5
BOTTOM_STRIP_H = 16
FPS = 30
INTRO_FRAMES = 90
PHASE_CARD_FRAMES = 90
END_FRAMES = 120
PHASE_SUMMARIES = {
    1: 'simulate dual patience insertion normally before freeze point @36.8% of input',
    2: 'continue with base array lengths frozen, redirect new pile values to overflow',
    3: 'flatten piles into contiguous runs with overflow band appended',
    4: 'sort the overflow pile',
}
INTRO_TITLE = 'Jessesort'
INTRO_SUMMARY = 'with early base array freezing and an overflow pile'
FLATTEN_FRAMES_PER_VALUE = 5
SCREEN_W, SCREEN_H = 1280, 720
FRAME_DIR = Path('./frames')
OUT_MP4 = Path('./jessesort_simulated_freezing_video.mp4')
OUT_WAV = Path('./jessesort_simulated_freezing_audio.wav')
TRACE = Path('./jessesort_simulated_freezing_trace.txt')
SCRIPT_COPY = Path('./jessesort_simulated_freezing_script.py')

# Two equal visual sections.
# Top section: original input stream.
# Work section: base arrays during insertion, flattened bars during reconstruction,
# and the local overflow sort during the final phase.
SECTION_X = 56
SECTION_W = SCREEN_W - 112
SECTION_H = 250
TOP_SECTION_Y = 120
WORK_SECTION_Y = 400
BAR_GAP = 6
MAX_BAR_H = 190
TAIL_CELL_H = 42
COLOR_OFFSET = 0.6
BG=(22,24,28); PANEL_BG=(34,37,43); TEXT=(230,230,230); MUTED=(160,165,175)
UNPROCESSED_BAR=(120,120,120); UNPROCESSED_STRIP=(82,82,82)
GAME0_STRIP=(0,0,0)          # game 0 = descending piles = black marker
GAME1_STRIP=(255,255,255)    # game 1 = ascending piles = white marker
OUTLINE=(18,18,18); WHITE_OUTLINE=(20,20,20); GAME0_GRAY_OUTLINE=(155,155,155)
CURRENT_OUTLINE=(255,220,80)
OVERFLOW_FILL=(135,135,135)
SHIFT_OUTLINE=(255,140,70)

# ================= SIMULATION =================
def make_values():
    rng=random.Random(RANDOM_SEED)
    values = rng.sample(range(1, NUM_INPUT_VALUES + 1), NUM_INPUT_VALUES)

    # Force a natural ascending run in the middle.
    run_len = 8
    run_start = (NUM_INPUT_VALUES - run_len) // 2
    values[run_start:run_start + run_len] = sorted(values[run_start:run_start + run_len])
    return values


def choose_game(values, i):
    # game 0 = descending piles; game 1 = ascending piles
    if i == 0: return 0
    return 1 if values[i] >= values[i-1] else 0


def find_ascending_pile(tails_descending, value):
    # ascending piles accept tail <= value; tails are kept descending.
    lo, hi = 0, len(tails_descending)
    while lo < hi:
        mid=(lo+hi)//2
        if tails_descending[mid] <= value:
            hi=mid
        else:
            lo=mid+1
    return lo


def find_descending_pile(tails_ascending, value):
    # descending piles accept tail >= value; tails are kept ascending.
    lo, hi = 0, len(tails_ascending)
    while lo < hi:
        mid=(lo+hi)//2
        if tails_ascending[mid] >= value:
            hi=mid
        else:
            lo=mid+1
    return lo


def find_pile_for_game(tails, game, value):
    return find_descending_pile(tails, value) if game == 0 else find_ascending_pile(tails, value)


class Sim:
    def __init__(self, values):
        self.values=values; self.n=len(values); self.max_value=max(values)
        self.assignments=[None]*self.n
        self.tails={0:[], 1:[]}
        self.current_i=-1; self.next_i=0; self.last_event=None; self.events=[]
        self.phase='normal_insert'
        self.frozen=False
        self.frozen_len={0:None, 1:None}
        self.overflow=[]
        self.flatten_visible=0
        self.flatten_current=None
        self.flattened_display=[]
        self.overflow_dest_start=None
        self.overflow_dest_len=0
        self.sort_current_dest=None
        self.sort_shift_dest=None
        self.sort_events=[]

    def overflow_pile_id(self):
        return self.frozen_len[1] if self.frozen_len[1] is not None else len(self.tails[1])

    def done(self):
        return self.next_i >= self.n

    def freeze(self):
        self.frozen=True
        self.frozen_len={0:len(self.tails[0]), 1:len(self.tails[1])}
        self.phase='frozen_insert'

    def _make_event(self, i, v, g, p, direction, action, old_tail=None, overflow_index=None):
        gname='descending piles' if g == 0 else 'ascending piles'
        e={
            'i':i,'value':v,'direction':direction,'game':g,'game_name':gname,'pile':p,
            'action': action, 'old_tail': old_tail, 'overflow_index': overflow_index,
            'phase': self.phase, 'tails0':list(self.tails[0]), 'tails1':list(self.tails[1]),
            'overflow_count': len(self.overflow)
        }
        self.last_event=e
        self.events.append(e)
        return e

    def step_normal(self):
        i=self.next_i; v=self.values[i]; g=choose_game(self.values,i)
        p=find_pile_for_game(self.tails[g], g, v)
        old_tail = None if p == len(self.tails[g]) else self.tails[g][p]
        if p == len(self.tails[g]):
            self.tails[g].append(v)
            action='new_tail'
        else:
            self.tails[g][p]=v
            action='replace_tail'
        self.assignments[i]={'kind':'normal','game':g,'pile':p}
        self.current_i=i; self.next_i+=1
        direction='start' if i==0 else ('up' if v>=self.values[i-1] else 'down')
        return self._make_event(i, v, g, p, direction, action, old_tail=old_tail)

    def step_frozen(self):
        i=self.next_i; v=self.values[i]; g=choose_game(self.values,i)
        p=find_pile_for_game(self.tails[g], g, v)
        direction='start' if i==0 else ('up' if v>=self.values[i-1] else 'down')
        if p < self.frozen_len[g]:
            old_tail = self.tails[g][p]
            self.tails[g][p]=v
            action='replace_tail_frozen'
            self.assignments[i]={'kind':'normal','game':g,'pile':p}
            overflow_index=None
            event_pile=p
        else:
            overflow_index=len(self.overflow)
            overflow_pile=self.overflow_pile_id()
            self.overflow.append({'src_i':i,'value':v,'orig_game':g,'target_pile':p,'overflow_index':overflow_index})
            self.assignments[i]={'kind':'overflow','game':1,'pile':overflow_pile,'overflow_index':overflow_index}
            old_tail=None
            action='overflow'
            event_pile=overflow_pile
        self.current_i=i; self.next_i+=1
        return self._make_event(i, v, g, event_pile, direction, action, old_tail=old_tail, overflow_index=overflow_index)

# ================= DRAWING =================
def pile_color(pile_id, game=0, color_offset=0.5):
    # Main bar color encodes pile id. Offset game 1's palette so
    # pile 0 does not start with the same red as game 0 pile 0.
    game_offset = 0.0 if game == 0 else color_offset
    # hue=(pile_id*0.61803398875 + game_offset)%1.0
    hue=(pile_id*0.2 + game_offset)%1.0
    r,g,b=colorsys.hsv_to_rgb(hue,0.87,0.92)
    return int(r*255), int(g*255), int(b*255)


def draw_text(surface, font, text, x, y, color=TEXT):
    surface.blit(font.render(text, True, color), (x,y))


def contrast_text_color(rgb):
    r, g, b = rgb
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return (245, 245, 245) if lum < 105 else (18, 18, 18)


def draw_centered_text(surface, font, text, rect, color):
    img = font.render(str(text), True, color)
    tx = rect.x + (rect.w - img.get_width()) // 2
    # ty = rect.y + (rect.h - img.get_height()) // 2
    ty = ty = rect.y + 3
    surface.blit(img, (tx, ty))


def draw_centered_value(surface, font, value, rect, fill_color):
    # txt_color = contrast_text_color(fill_color)
    txt_color = (18, 18, 18)
    draw_centered_text(surface, font, str(value), rect, txt_color)


def draw_marker(surface, rect, game):
    if game == 0:
        pygame.draw.rect(surface, GAME0_STRIP, rect)
        pygame.draw.rect(surface, GAME0_GRAY_OUTLINE, rect, 2)
    else:
        pygame.draw.rect(surface, GAME1_STRIP, rect)
        pygame.draw.rect(surface, WHITE_OUTLINE, rect, 2)


def draw_header(surface, fonts, sim):
    title, small, tiny=fonts
    draw_text(surface,title,'Jessesort with early freeze and overflow',48,28)
    phase = getattr(sim, 'phase', 'normal_insert')
    if phase == 'normal_insert':
        if sim.last_event is None:
            status=f'phase 1: normal insertion until freeze point. new piles can be added.'
        else:
            e=sim.last_event
            status=f"i={e['i']:02d} value={e['value']:02d} direction(i-1|i)={e['direction']}→game={e['game']} pile={e['pile']}"
    elif phase == 'frozen_insert':
        if sim.last_event is None or sim.next_i == FREEZE_POINT:
            status=f'phase 2: base lengths frozen. game0={sim.frozen_len[0]}, game1={sim.frozen_len[1]}. use overflow pile.'
        else:
            e=sim.last_event
            if e['action'] == 'overflow':
                status=f"phase 2: i={e['i']:02d} value={e['value']:02d} would create new pile in game {e['game']} → overflow count {e['overflow_count']}"
            else:
                status=f"phase 2: i={e['i']:02d} value={e['value']:02d} game={e['game']} pile={e['pile']} updates frozen tail {e.get('old_tail')}→{e['value']}"
    elif phase == 'flatten':
        status=f"phase 3: flatten piles."
        cur = getattr(sim, 'flatten_current', None)
        if cur is not None:
            status += f" src={cur['src_i']} value={cur['value']} -> dest={cur['dest']}"
    elif phase == 'sort_overflow':
        status=f'phase 4: sort overflow pile.'
    elif phase == 'complete':
        status='complete.'
    # else:
    #     status='complete.'
    draw_text(surface,small,status,48,66,MUTED)

    x,y=860,30
    draw_text(surface,small,'Bottom marker:',x,y)
    r0=pygame.Rect(x,y+32,44,18); draw_marker(surface,r0,0); draw_text(surface,tiny,'game 0: descending piles',x+55,y+29,MUTED)
    r1=pygame.Rect(x,y+56,44,18); draw_marker(surface,r1,1); draw_text(surface,tiny,'game 1: ascending piles / overflow',x+55,y+53,MUTED)


def section_cell_geometry(n):
    usable_w = SECTION_W - BAR_GAP * (n - 1)
    cell_w = usable_w // n
    return SECTION_X, cell_w, BAR_GAP


def bar_height(sim, value):
    return max(BOTTOM_STRIP_H + 4, int(((value + VISUAL_HEIGHT_BOOST) / (sim.max_value + VISUAL_HEIGHT_BOOST)) * MAX_BAR_H))


def value_bar_rect_for_value(sim, value, dest_index, section_y):
    x0, cell_w, gap = section_cell_geometry(sim.n)
    baseline = section_y + SECTION_H - 12
    h = bar_height(sim, value)
    x = x0 + dest_index * (cell_w + gap)
    return pygame.Rect(x, baseline - h, cell_w, h)


def value_bar_rect(sim, index, section_y):
    return value_bar_rect_for_value(sim, sim.values[index], index, section_y)


def assignment_style(assignment):
    if assignment is None:
        return UNPROCESSED_BAR, None
    if assignment.get('kind') == 'overflow':
        return OVERFLOW_FILL, 1
    return pile_color(assignment['pile'], assignment['game'], COLOR_OFFSET), assignment['game']


def draw_value_bar(surface, font, sim, index, section_y, assignment=None, outline_current=False):
    value = sim.values[index]
    full = value_bar_rect(sim, index, section_y)
    main = pygame.Rect(full.x, full.y, full.w, max(1, full.h - BOTTOM_STRIP_H))
    strip = pygame.Rect(full.x, full.bottom - BOTTOM_STRIP_H, full.w, BOTTOM_STRIP_H)

    if assignment is None:
        main_color = UNPROCESSED_BAR
        pygame.draw.rect(surface, main_color, main)
        pygame.draw.rect(surface, UNPROCESSED_STRIP, strip)
        pygame.draw.rect(surface, OUTLINE, strip, 1)
    else:
        main_color, marker_game = assignment_style(assignment)
        pygame.draw.rect(surface, main_color, main)
        draw_marker(surface, strip, marker_game)

    draw_centered_value(surface, font, value, main, main_color)
    pygame.draw.rect(surface, OUTLINE, full, 1)
    if outline_current:
        pygame.draw.rect(surface, CURRENT_OUTLINE, full.inflate(8, 8), 4)


def draw_bars(surface, fonts, sim):
    _, small, tiny = fonts
    panel = pygame.Rect(48, TOP_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    if getattr(sim, 'phase', 'insert') in ('normal_insert','frozen_insert'):
        display_message = 'unsorted input→simulation blueprint'
    # elif getattr(sim, 'phase', 'insert') == 'flatten':
    else:
        display_message = 'simulation blueprint→flat array'
    draw_text(surface, small, display_message, 64, TOP_SECTION_Y + 10, TEXT)

    flatten_cur = getattr(sim, 'flatten_current', None)
    phase = getattr(sim, 'phase', 'normal_insert')
    for i in range(sim.n):
        is_current_insert = (phase in ('normal_insert','frozen_insert') and i == sim.current_i)
        is_current_flatten_source = (phase == 'flatten' and flatten_cur is not None and i == flatten_cur['src_i'])
        draw_value_bar(surface, tiny, sim, i, TOP_SECTION_Y, sim.assignments[i],
                       outline_current=(is_current_insert or is_current_flatten_source))

    # Freeze boundary marker after the final phase-1 value.
    x0, cell_w, gap = section_cell_geometry(sim.n)
    if 0 < FREEZE_POINT < sim.n:
        bx = x0 + FREEZE_POINT * (cell_w + gap) - gap // 2
        pygame.draw.line(surface, CURRENT_OUTLINE, (bx, TOP_SECTION_Y + 42), (bx, TOP_SECTION_Y + SECTION_H - 10), 2)
        draw_text(surface, tiny, 'freeze', bx + 4, TOP_SECTION_Y + 44, CURRENT_OUTLINE)


def build_flatten_order(sim):
    layout, _ = build_flatten_layout(sim)
    return [item for item in layout if item is not None]


def build_flatten_layout(sim):
    grouped_counts = {0: {}, 1: {}}
    for assignment in sim.assignments:
        if assignment is None:
            continue
        game = assignment['game']
        pile = assignment['pile']
        grouped_counts[game][pile] = grouped_counts[game].get(pile, 0) + 1

    region_start = {0: {}, 1: {}}
    region_len = {0: {}, 1: {}}
    pos = 0
    for game in (0, 1):
        for pile in sorted(grouped_counts[game]):
            count = grouped_counts[game][pile]
            region_start[game][pile] = pos
            region_len[game][pile] = count
            pos += count

    layout = [None] * sim.n
    write_cursor = {0: {}, 1: {}}
    for pile in region_start[0]:
        write_cursor[0][pile] = region_start[0][pile] + region_len[0][pile] - 1
    for pile in region_start[1]:
        write_cursor[1][pile] = region_start[1][pile]

    reveal_events = []
    for src_i, assignment in enumerate(sim.assignments):
        if assignment is None:
            continue
        game = assignment['game']
        pile = assignment['pile']
        value = sim.values[src_i]
        dest = write_cursor[game][pile]
        kind = assignment.get('kind', 'normal')
        item = {'src_i':src_i, 'value':value, 'game':game, 'pile':pile, 'dest':dest, 'kind':kind}
        layout[dest] = item
        reveal_events.append(item)
        if game == 0:
            write_cursor[0][pile] -= 1
        else:
            # This includes overflow: it is appended as the final game-1 pile and written in encountered order.
            write_cursor[1][pile] += 1

    return layout, reveal_events


def draw_overflow_cell(surface, fonts, sim, rect, highlighted=False):
    _, small, tiny = fonts
    pygame.draw.rect(surface, OVERFLOW_FILL, rect)
    bit = pygame.Rect(rect.x, rect.bottom - BOTTOM_STRIP_H, rect.w, BOTTOM_STRIP_H)
    draw_marker(surface, bit, 1)
    pygame.draw.rect(surface, OUTLINE, rect, 1)
    draw_centered_text(surface, tiny, 'overflow', pygame.Rect(rect.x, rect.y, rect.w, rect.h - BOTTOM_STRIP_H), (18,18,18))
    if highlighted:
        pygame.draw.rect(surface, CURRENT_OUTLINE, rect.inflate(8, 8), 4)


def draw_base_game_row(surface, fonts, sim, game, rect):
    _, small, tiny = fonts
    pygame.draw.rect(surface, (29, 32, 38), rect, border_radius=8)
    pygame.draw.rect(surface, (50, 54, 62), rect, 1, border_radius=8)

    label = 'game 0: descending piles | tails ascending' if game == 0 else 'game 1: ascending piles | tails descending'
    if sim.frozen:
        label += ' | frozen'
    draw_text(surface, small, label, rect.x + 12, rect.y + 8, TEXT)
    draw_marker(surface, pygame.Rect(rect.right - 55, rect.y + 12, 36, 16), game)

    tails = sim.tails[game]
    show_overflow = (game == 1 and sim.frozen)
    overflow_w = 108 if show_overflow else 0
    overflow_gap = 10 if show_overflow and tails else 0

    highlighted_tail = None
    highlighted_overflow = False
    if getattr(sim, 'phase', '') in ('normal_insert', 'frozen_insert') and sim.last_event is not None:
        if sim.last_event.get('action') == 'overflow' and game == 1:
            highlighted_overflow = True
        elif sim.last_event.get('game') == game and sim.last_event.get('action') != 'overflow':
            highlighted_tail = sim.last_event.get('pile')

    if not tails and not show_overflow:
        draw_text(surface, tiny, 'no piles yet', rect.x + 12, rect.y + 48, MUTED)
        return

    gap = 6
    count = len(tails)
    available_w = rect.w - 24 - overflow_w - overflow_gap - max(0, count - 1) * gap
    cell_w = 42
    if count:
        cell_w = max(24, min(58, available_w // count))
    total_w = count * cell_w + max(0, count - 1) * gap + overflow_w + overflow_gap
    cx = rect.x + 12
    if total_w < rect.w - 24:
        cx = rect.x + 12 + ((rect.w - 24) - total_w) // 2
    cy = rect.y + 44

    for pile, tail in enumerate(tails):
        r = pygame.Rect(cx, cy, cell_w, TAIL_CELL_H)
        fill = pile_color(pile, game, COLOR_OFFSET)
        pygame.draw.rect(surface, fill, r)
        bit = pygame.Rect(cx, cy + TAIL_CELL_H - BOTTOM_STRIP_H, cell_w, BOTTOM_STRIP_H)
        draw_marker(surface, bit, game)
        pygame.draw.rect(surface, OUTLINE, r, 1)
        draw_centered_value(surface, tiny, tail, pygame.Rect(cx, cy, cell_w, TAIL_CELL_H - BOTTOM_STRIP_H), fill)
        if highlighted_tail == pile:
            pygame.draw.rect(surface, CURRENT_OUTLINE, r.inflate(8, 8), 4)
        cx += cell_w + gap

    if show_overflow:
        if count:
            cx += overflow_gap - gap
        r = pygame.Rect(cx, cy, overflow_w, TAIL_CELL_H)
        draw_overflow_cell(surface, fonts, sim, r, highlighted=highlighted_overflow)
        if sim.overflow:
            draw_text(surface, tiny, f'n={len(sim.overflow)}', r.right + 8, cy + 10, MUTED)


def draw_work_area_insert(surface, fonts, sim):
    _, small, tiny = fonts
    panel = pygame.Rect(48, WORK_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    if sim.frozen:
        label = 'phase 2: frozen base arrays. new piles redirect to one game-1 overflow pile.'
    else:
        label = 'phase 1: base arrays / current pile tails'
    draw_text(surface, small, label, 64, WORK_SECTION_Y + 10, TEXT)

    inner_x = 64
    inner_w = SCREEN_W - 128
    row_gap = 12
    row_y = WORK_SECTION_Y + 38
    row_h = (SECTION_H - 54 - row_gap) // 2
    game0_rect = pygame.Rect(inner_x, row_y, inner_w, row_h)
    game1_rect = pygame.Rect(inner_x, row_y + row_h + row_gap, inner_w, row_h)
    draw_base_game_row(surface, fonts, sim, 0, game0_rect)
    draw_base_game_row(surface, fonts, sim, 1, game1_rect)


def flattened_item_style(item):
    if item.get('kind') == 'overflow':
        return OVERFLOW_FILL, 1
    return pile_color(item['pile'], item['game'], COLOR_OFFSET), item['game']


def draw_flattened_item(surface, fonts, sim, item, outline=False, shift_outline=False):
    _, small, tiny = fonts
    value = item['value']
    dest = item['dest']
    full = value_bar_rect_for_value(sim, value, dest, WORK_SECTION_Y)
    main = pygame.Rect(full.x, full.y, full.w, max(1, full.h - BOTTOM_STRIP_H))
    bit = pygame.Rect(full.x, full.bottom - BOTTOM_STRIP_H, full.w, BOTTOM_STRIP_H)
    fill, marker_game = flattened_item_style(item)
    pygame.draw.rect(surface, fill, main)
    draw_marker(surface, bit, marker_game)
    pygame.draw.rect(surface, OUTLINE, full, 1)
    draw_centered_value(surface, tiny, value, main, fill)
    if shift_outline:
        pygame.draw.rect(surface, SHIFT_OUTLINE, full.inflate(8, 8), 4)
    if outline:
        pygame.draw.rect(surface, CURRENT_OUTLINE, full.inflate(8, 8), 4)


def draw_flatten_guides(surface, sim):
    x0, cell_w, gap = section_cell_geometry(sim.n)
    guide_top = WORK_SECTION_Y + 52
    guide_h = SECTION_H - 64
    for k in range(sim.n):
        sx = x0 + k * (cell_w + gap)
        guide = pygame.Rect(sx, guide_top, cell_w, guide_h)
        pygame.draw.rect(surface, (52, 55, 64), guide, 1)


def draw_work_area_flatten(surface, fonts, sim, visible_count):
    _, small, tiny = fonts
    panel = pygame.Rect(48, WORK_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    draw_text(surface, small, 'phase 3: flatten runs. overflow appended as final game-1 pile', 64, WORK_SECTION_Y + 10, TEXT)
    draw_text(surface, tiny, 'scan left-to-right. game 0 writes R→L, game 1 and overflow write L→R in encountered order', 64, WORK_SECTION_Y + 34, MUTED)
    draw_flatten_guides(surface, sim)

    _, reveal_events = build_flatten_layout(sim)
    for item in reveal_events[:visible_count]:
        cur = getattr(sim, 'flatten_current', None)
        outline = cur is not None and item['src_i'] == cur['src_i'] and item['dest'] == cur['dest']
        draw_flattened_item(surface, fonts, sim, item, outline=outline)


def draw_work_area_sort_overflow(surface, fonts, sim):
    _, small, tiny = fonts
    panel = pygame.Rect(48, WORK_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    draw_text(surface, small, 'phase 4: sort overflow pile', 64, WORK_SECTION_Y + 10, TEXT)
    draw_text(surface, tiny, 'can use any fast sorting algorithm for this single pile', 64, WORK_SECTION_Y + 34, MUTED)
    draw_flatten_guides(surface, sim)

    for item in sim.flattened_display:
        if item is None:
            continue
        outline = (sim.sort_current_dest is not None and item['dest'] == sim.sort_current_dest)
        shift_outline = (sim.sort_shift_dest is not None and item['dest'] == sim.sort_shift_dest)
        draw_flattened_item(surface, fonts, sim, item, outline=outline, shift_outline=shift_outline)


def draw_work_area(surface, fonts, sim):
    phase = getattr(sim, 'phase', 'normal_insert')
    if phase == 'flatten':
        draw_work_area_flatten(surface, fonts, sim, getattr(sim, 'flatten_visible', 0))
    elif phase in ('sort_overflow', 'complete'):
        draw_work_area_sort_overflow(surface, fonts, sim)
    else:
        draw_work_area_insert(surface, fonts, sim)


def draw_intro_card_overlay(surface, fonts):
    title, small, tiny = fonts
    card_w, card_h = 860, 180
    card = pygame.Rect((SCREEN_W - card_w) // 2, (SCREEN_H - card_h) // 2, card_w, card_h)
    shadow = card.move(6, 6)
    pygame.draw.rect(surface, (8, 9, 11), shadow, border_radius=14)
    pygame.draw.rect(surface, (245, 245, 245), card, border_radius=14)
    pygame.draw.rect(surface, CURRENT_OUTLINE, card, 4, border_radius=14)
    title_img = title.render(INTRO_TITLE, True, (18, 18, 18))
    summary_img = small.render(INTRO_SUMMARY, True, (18, 18, 18))
    surface.blit(title_img, (card.x + (card.w - title_img.get_width()) // 2, card.y + 42))
    surface.blit(summary_img, (card.x + (card.w - summary_img.get_width()) // 2, card.y + 102))


def draw_intro_card_frame(surface, fonts, sim):
    draw_frame(surface, fonts, sim)
    draw_intro_card_overlay(surface, fonts)


def draw_phase_card_overlay(surface, fonts, phase_num, summary):
    title, small, tiny = fonts
    card_w, card_h = 920, 180
    card = pygame.Rect((SCREEN_W - card_w) // 2, (SCREEN_H - card_h) // 2, card_w, card_h)
    shadow = card.move(6, 6)
    pygame.draw.rect(surface, (8, 9, 11), shadow, border_radius=14)
    pygame.draw.rect(surface, (245, 245, 245), card, border_radius=14)
    pygame.draw.rect(surface, CURRENT_OUTLINE, card, 4, border_radius=14)
    phase_text = f'phase {phase_num}'
    phase_img = title.render(phase_text, True, (18, 18, 18))
    summary_img = small.render(summary, True, (18, 18, 18))
    surface.blit(phase_img, (card.x + (card.w - phase_img.get_width()) // 2, card.y + 42))
    surface.blit(summary_img, (card.x + (card.w - summary_img.get_width()) // 2, card.y + 102))


def draw_phase_card_frame(surface, fonts, sim, phase_num):
    draw_frame(surface, fonts, sim)
    draw_phase_card_overlay(surface, fonts, phase_num, PHASE_SUMMARIES[phase_num])


def draw_frame(surface, fonts, sim):
    surface.fill(BG)
    draw_header(surface, fonts, sim)
    draw_bars(surface, fonts, sim)
    draw_work_area(surface, fonts, sim)

# ================= AUDIO =================
def add_tone(data, sr, start_frame, value, game, kind='insert'):
    start = int((start_frame / FPS) * sr)
    norm = (value - 1) / max(1, NUM_INPUT_VALUES - 1)
    # Phase 3 must use the exact same tone as phases 1 and 2.
    # Treat flatten events as ordinary insertion tones; only phase 4 has distinct audio.
    if kind == 'sort':
        dur = int(0.040 * sr)
        freq = (240 + norm * 620) * 1.04
        volume = 0.14
        overtone = 0.05
    else:
        dur = int(0.055 * sr)
        freq = (220 + norm * 660) * (0.92 if game == 0 else 1.12)
        volume = 0.22
        overtone = 0.0

    attack = 200
    release = 700
    for j in range(dur):
        idx = start + j
        if idx >= len(data):
            break
        env = 1.0
        if j < attack:
            env = j / attack
        elif j > dur - release:
            env = max(0.0, (dur - j) / release)
        sample = math.sin(2 * math.pi * freq * j / sr)
        if overtone:
            sample = (1.0 - overtone) * sample + overtone * math.sin(2 * math.pi * freq * 2.0 * j / sr)
        data[idx] += volume * env * sample


def synth_audio(audio_events, total_frames):
    sr = 44100
    total_samples = int((total_frames / FPS) * sr) + sr // 2
    data = [0.0] * total_samples
    for e in audio_events:
        add_tone(data, sr, e['start_frame'], e['value'], e.get('game', 1), kind=e.get('kind','insert'))
    with wave.open(str(OUT_WAV), 'wb') as wf:
        wf.setnchannels(2)
        wf.setsampwidth(2)
        wf.setframerate(sr)
        frames = []
        for s in data:
            val = max(-1, min(1, s))
            q = int(val * 32767)
            frames.append(struct.pack('<hh', q, q))
        wf.writeframes(b''.join(frames))

# ================= PHASE 4 SORT =================
def prepare_flattened_display(sim):
    layout, reveal_events = build_flatten_layout(sim)
    sim.flattened_display = [dict(item) if item is not None else None for item in layout]
    overflow_dests = [item['dest'] for item in layout if item is not None and item.get('kind') == 'overflow']
    if overflow_dests:
        sim.overflow_dest_start = min(overflow_dests)
        sim.overflow_dest_len = len(overflow_dests)
    else:
        sim.overflow_dest_start = None
        sim.overflow_dest_len = 0
    return layout, reveal_events


def render_hold(surface, fonts, sim, frame_idx, frames):
    for _ in range(frames):
        draw_frame(surface,fonts,sim)
        pygame.image.save(surface, FRAME_DIR/f'frame_{frame_idx:05d}.png')
        frame_idx += 1
    return frame_idx


def render_intro_card(surface, fonts, sim, frame_idx, frames=INTRO_FRAMES):
    for _ in range(frames):
        draw_intro_card_frame(surface, fonts, sim)
        pygame.image.save(surface, FRAME_DIR/f'frame_{frame_idx:05d}.png')
        frame_idx += 1
    return frame_idx


def render_phase_card(surface, fonts, sim, frame_idx, phase_num, frames=PHASE_CARD_FRAMES):
    for _ in range(frames):
        draw_phase_card_frame(surface, fonts, sim, phase_num)
        pygame.image.save(surface, FRAME_DIR/f'frame_{frame_idx:05d}.png')
        frame_idx += 1
    return frame_idx


def render_overflow_final_placement(surface, fonts, sim, frame_idx, audio_events):
    """Phase 4: one linear visual pass over the overflow segment.

    Instead of animating insertion-sort shifts, precompute the sorted overflow order
    and place the value that belongs at each overflow destination. This keeps phase 4
    short while still showing the final sorted positions.
    """
    sim.phase = 'sort_overflow'
    sim.sort_current_dest = None
    sim.sort_shift_dest = None
    if not sim.overflow_dest_len:
        return frame_idx

    start = sim.overflow_dest_start
    end = start + sim.overflow_dest_len
    arr = sim.flattened_display
    sorted_items = sorted((dict(arr[k]) for k in range(start, end)), key=lambda item: item['value'])

    for offset, item in enumerate(sorted_items):
        dest = start + offset
        placed = dict(item)
        placed['dest'] = dest
        placed['kind'] = 'overflow'
        placed['game'] = 1
        placed['pile'] = sim.overflow_pile_id()
        arr[dest] = placed
        sim.sort_current_dest = dest
        sim.sort_shift_dest = None
        audio_events.append({'start_frame': frame_idx, 'value': placed['value'], 'game':1, 'kind':'sort'})
        frame_idx = render_hold(surface, fonts, sim, frame_idx, SORT_FRAMES_PER_EVENT)

    sim.sort_current_dest = None
    sim.sort_shift_dest = None
    return frame_idx

# ================= MAIN =================
def main():
    pygame.init(); pygame.font.init()
    surface=pygame.Surface((SCREEN_W,SCREEN_H))
    fonts=(pygame.font.SysFont('consolas',28,bold=True), pygame.font.SysFont('consolas',18), pygame.font.SysFont('consolas',14))
    if FRAME_DIR.exists(): shutil.rmtree(FRAME_DIR)
    FRAME_DIR.mkdir(parents=True)
    sim=Sim(make_values())
    frame_idx=0
    audio_events=[]

    # 30-frame intro card, then the phase 1 card and normal insertion through the first 36.8% of input.
    sim.phase = 'normal_insert'
    frame_idx = render_intro_card(surface, fonts, sim, frame_idx)
    frame_idx = render_phase_card(surface, fonts, sim, frame_idx, 1)
    while sim.next_i < min(FREEZE_POINT, sim.n):
        print(f"sim step: {sim.current_i + 2}/{sim.n}")
        e = sim.step_normal()
        audio_events.append({'start_frame': frame_idx, 'value': e['value'], 'game': e['game'], 'kind':'insert'})
        frame_idx = render_hold(surface, fonts, sim, frame_idx, FRAMES_PER_STEP)

    # Phase 2 card, then freeze base-array lengths and redirect new piles to overflow.
    print("freeze")
    sim.freeze()
    frame_idx = render_phase_card(surface, fonts, sim, frame_idx, 2)
    while not sim.done():
        print(f"sim step: {sim.current_i + 2}/{sim.n}")
        e = sim.step_frozen()
        audio_events.append({'start_frame': frame_idx, 'value': e['value'], 'game': (1 if e['action'] == 'overflow' else e['game']), 'kind':'insert'})
        frame_idx = render_hold(surface, fonts, sim, frame_idx, FRAMES_PER_STEP)

    # Phase 3 card, then replace base-array work area with flattened reconstruction.
    sim.phase = 'flatten'
    sim.current_i = -1
    sim.flatten_visible = 0
    sim.flatten_current = None
    frame_idx = render_phase_card(surface, fonts, sim, frame_idx, 3)
    _, flatten_reveal_events = build_flatten_layout(sim)
    for visible in range(1, sim.n + 1):
        print(f"flatten step: {visible}/{sim.n}")
        sim.flatten_visible = visible
        sim.flatten_current = flatten_reveal_events[visible - 1]
        cur = sim.flatten_current
        audio_events.append({'start_frame': frame_idx, 'value': cur['value'], 'game': cur['game'], 'kind':'insert'})
        frame_idx = render_hold(surface, fonts, sim, frame_idx, FLATTEN_FRAMES_PER_VALUE)
    sim.flatten_current = None
    prepare_flattened_display(sim)

    # Phase 4 card, then do one final-placement pass over the appended overflow pile.
    sim.phase = 'sort_overflow'
    sim.sort_current_dest = None
    sim.sort_shift_dest = None
    frame_idx = render_phase_card(surface, fonts, sim, frame_idx, 4)
    frame_idx = render_overflow_final_placement(surface, fonts, sim, frame_idx, audio_events)

    # ending hold: keep the final sorted overflow state visible.
    sim.phase = 'complete'
    sim.sort_current_dest = None
    sim.sort_shift_dest = None
    frame_idx = render_hold(surface, fonts, sim, frame_idx, END_FRAMES)
    print("adding audio")
    synth_audio(audio_events, frame_idx)
    with open(TRACE,'w') as f:
        f.write(f'values={sim.values}\n')
        f.write(f'NUM_INPUT_VALUES={NUM_INPUT_VALUES}\nFREEZE_ALPHA={FREEZE_ALPHA}\nFREEZE_POINT={FREEZE_POINT}\n')
        f.write(f'VISUAL_HEIGHT_BOOST={VISUAL_HEIGHT_BOOST}\nFRAMES_PER_STEP={FRAMES_PER_STEP}\nPHASE_CARD_FRAMES={PHASE_CARD_FRAMES}\n')
        f.write(f'BOTTOM_STRIP_H={BOTTOM_STRIP_H}\nTAIL_CELL_H={TAIL_CELL_H}\nFLATTEN_FRAMES_PER_VALUE={FLATTEN_FRAMES_PER_VALUE}\nSORT_FRAMES_PER_EVENT={SORT_FRAMES_PER_EVENT}\n')
        f.write('game 0 = descending piles / black marker with gray outline\n')
        f.write('game 1 = ascending piles / white marker / palette offset by 0.5 hue\n')
        f.write('phase 1: normal insertion until freeze point; base arrays can grow\n')
        f.write('phase 2: base-array lengths frozen; would-be new piles redirect to one gray overflow cell at end of game 1\n')
        f.write('phase 3: flattening treats overflow as final game-1 pile and writes it left-to-right in encountered order\n')
        f.write('phase 4: one pass places overflow values into their final sorted slots; no shift animation\n')
        f.write('intro card: 30 frames describing Jessesort with early base array freezing and an overflow pile\ntransition cards: 30 frames before each phase, including phase 1\n')
        f.write('final hold: renders from sorted flattened_display; no reversion to pre-sort layout\n')
        f.write(f'frozen_len0={sim.frozen_len[0]} frozen_len1={sim.frozen_len[1]} overflow_pile_id={sim.overflow_pile_id()}\n')
        f.write(f'overflow_count={len(sim.overflow)} overflow_values={[x["value"] for x in sim.overflow]} overflow_sources={[x["src_i"] for x in sim.overflow]}\n')
        for e in sim.events:
            f.write(f"i={e['i']} value={e['value']} phase={e['phase']} direction={e['direction']} game={e['game']} pile={e['pile']} action={e['action']} old_tail={e['old_tail']} overflow_index={e['overflow_index']} tails0={e['tails0']} tails1={e['tails1']} overflow_count={e['overflow_count']}\n")
        f.write(f'flatten_order={[(item["src_i"], item["value"], item["game"], item["pile"], item["dest"], item["kind"]) for item in build_flatten_order(sim)]}\n')
        layout, reveal_events = build_flatten_layout(sim)
        f.write(f'reveal_events={[(item["src_i"], item["value"], item["game"], item["pile"], item["dest"], item["kind"]) for item in reveal_events]}\n')
        final_overflow = []
        if sim.overflow_dest_start is not None:
            final_overflow = [sim.flattened_display[k]['value'] for k in range(sim.overflow_dest_start, sim.overflow_dest_start + sim.overflow_dest_len)]
        f.write(f'overflow_dest_start={sim.overflow_dest_start} overflow_dest_len={sim.overflow_dest_len} final_sorted_overflow={final_overflow}\n')
        f.write(f'total_frames={frame_idx}\n')

    cmd=['ffmpeg','-y','-r',str(FPS),'-i',str(FRAME_DIR/'frame_%05d.png'),'-i',str(OUT_WAV),'-c:v','libx264','-pix_fmt','yuv420p','-c:a','aac','-shortest',str(OUT_MP4)]
    ffmpeg_log = TRACE.with_suffix('.ffmpeg.log')
    with open(ffmpeg_log, 'w') as log:
        subprocess.run(cmd, check=True, stdout=log, stderr=log)
    try:
        SCRIPT_COPY.write_text(Path(__file__).read_text())
    except Exception:
        pass
    print('WROTE', OUT_MP4)
    print('WROTE', TRACE)
    print('frames', frame_idx)
    print('values', sim.values)
    print('freeze_point', FREEZE_POINT, 'frozen_len', sim.frozen_len, 'overflow_count', len(sim.overflow))

if __name__=='__main__': main()
