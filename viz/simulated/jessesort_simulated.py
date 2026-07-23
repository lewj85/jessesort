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
FRAMES_PER_STEP = 5
BOTTOM_STRIP_H = 16
FPS = 30
INTRO_FRAMES = 90
PHASE_CARD_FRAMES = 90
END_FRAMES = 120
PHASE_SUMMARIES = {
    1: 'simulate dual patience insertion',
    2: 'flatten piles into contiguous runs',
}
INTRO_TITLE = 'Jessesort'
INTRO_SUMMARY = 'with simulated insertion and flattening'
SIM_HOLD_FRAMES = 30
FLATTEN_HOLD_FRAMES = 30
SCREEN_W, SCREEN_H = 1280, 720
FRAME_DIR = Path('./frames')
OUT_MP4 = Path('./jessesort_simulated_video.mp4')
OUT_WAV = Path('./jessesort_simulated_audio.wav')
TRACE = Path('./jessesort_simulated_trace.txt')
SCRIPT_COPY = Path('./jessesort_simulated_script.py')

# Two equal visual sections.
# Top section: original input stream.
# Work section: base arrays during insertion, then flattened bars during reconstruction.
SECTION_X = 56
SECTION_W = SCREEN_W - 112
SECTION_H = 250
TOP_SECTION_Y = 120
WORK_SECTION_Y = 400
BAR_GAP = 6
MAX_BAR_H = 190
INSERTION_TO_FLATTEN_HOLD_FRAMES = 30
TAIL_CELL_H = 42
COLOR_OFFSET = 0.56
BG=(22,24,28); PANEL_BG=(34,37,43); TEXT=(230,230,230); MUTED=(160,165,175)
UNPROCESSED_BAR=(120,120,120); UNPROCESSED_STRIP=(82,82,82)
GAME0_STRIP=(0,0,0)          # game 0 = descending piles = black marker
GAME1_STRIP=(255,255,255)    # game 1 = ascending piles = white marker
OUTLINE=(18,18,18); WHITE_OUTLINE=(20,20,20); GAME0_GRAY_OUTLINE=(155,155,155)
CURRENT_OUTLINE=(255,220,80)

# ================= SIMULATION =================
def make_values():
    rng=random.Random(RANDOM_SEED)
    values = rng.sample(
        range(1, NUM_INPUT_VALUES + 1),
        NUM_INPUT_VALUES,
    )

    # Force a natural ascending run in the middle.
    # With NUM_INPUT_VALUES=40 and run_len=8, this gives
    # 16 random values, 8 sorted values, 16 random values.
    run_len = 8
    run_start = (NUM_INPUT_VALUES - run_len) // 2
    values[run_start:run_start + run_len] = sorted(values[run_start:run_start + run_len])

    return values

def choose_game(values, i):
    # game 0 = descending piles; game 1 = ascending piles
    if i == 0: return 0
    return 1 if values[i] >= values[i-1] else 0

def insert_ascending_piles(tails_descending, value):
    # ascending piles accept tail <= value; tails are kept descending.
    lo, hi = 0, len(tails_descending)
    while lo < hi:
        mid=(lo+hi)//2
        if tails_descending[mid] <= value:
            hi=mid
        else:
            lo=mid+1
    pile=lo
    old_tail = None if pile == len(tails_descending) else tails_descending[pile]
    if pile == len(tails_descending):
        tails_descending.append(value)
        action = 'new'
    else:
        tails_descending[pile]=value
        action = 'replace'
    return pile, old_tail, action

def insert_descending_piles(tails_ascending, value):
    # descending piles accept tail >= value; tails are kept ascending.
    lo, hi = 0, len(tails_ascending)
    while lo < hi:
        mid=(lo+hi)//2
        if tails_ascending[mid] >= value:
            hi=mid
        else:
            lo=mid+1
    pile=lo
    old_tail = None if pile == len(tails_ascending) else tails_ascending[pile]
    if pile == len(tails_ascending):
        tails_ascending.append(value)
        action = 'new'
    else:
        tails_ascending[pile]=value
        action = 'replace'
    return pile, old_tail, action

class Sim:
    def __init__(self, values):
        self.values=values; self.n=len(values); self.max_value=max(values)
        self.assignments=[None]*self.n
        self.tails={0:[], 1:[]}
        self.current_i=-1; self.next_i=0; self.last_event=None; self.events=[]
    def done(self): return self.next_i >= self.n
    def step(self):
        i=self.next_i; v=self.values[i]; g=choose_game(self.values,i)
        if g==0:
            p, old_tail, tail_action = insert_descending_piles(self.tails[0], v)
            gname='descending piles'
        else:
            p, old_tail, tail_action = insert_ascending_piles(self.tails[1], v)
            gname='ascending piles'
        self.assignments[i]=(g,p); self.current_i=i; self.next_i+=1
        direction='start' if i==0 else ('up' if v>=self.values[i-1] else 'down')
        e={'i':i,'value':v,'direction':direction,'game':g,'game_name':gname,'pile':p,
           'old_tail': old_tail, 'tail_action': tail_action,
           'tails0':list(self.tails[0]), 'tails1':list(self.tails[1])}
        self.last_event=e; self.events.append(e); return e

# ================= DRAWING =================
def pile_color(pile_id, game=0, color_offset=0.5):
    # Main bar color encodes pile id. Offset game 1's palette so
    # pile 0 does not start with the same red as game 0 pile 0.
    game_offset = 0.0 if game == 0 else color_offset
    # hue=(pile_id*0.61803398875 + game_offset)%1.0
    hue=(pile_id*0.11 + game_offset)%1.0
    r,g,b=colorsys.hsv_to_rgb(hue,0.87,0.92)
    return int(r*255), int(g*255), int(b*255)

def draw_text(surface, font, text, x, y, color=TEXT):
    surface.blit(font.render(text, True, color), (x,y))

def contrast_text_color(rgb):
    r, g, b = rgb
    # Perceptual luminance approximation. Use light text on dark fills, dark text on light fills.
    lum = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return (245, 245, 245) if lum < 105 else (18, 18, 18)

def draw_centered_value(surface, font, value, rect, fill_color):
    # txt_color = contrast_text_color(fill_color)
    txt_color = (18, 18, 18)
    img = font.render(str(value), True, txt_color)
    tx = rect.x + (rect.w - img.get_width()) // 2
    ty = rect.y + 3
    surface.blit(img, (tx, ty))

def draw_marker(surface, rect, game):
    if game == 0:
        pygame.draw.rect(surface, GAME0_STRIP, rect)
        pygame.draw.rect(surface, GAME0_GRAY_OUTLINE, rect, 2)  # requested gray outline for black marker
    else:
        pygame.draw.rect(surface, GAME1_STRIP, rect)
        pygame.draw.rect(surface, WHITE_OUTLINE, rect, 2)

def draw_header(surface, fonts, sim):
    title, small, tiny=fonts
    draw_text(surface,title,'Jessesort simulated insertion, then flattening',48,28)
    if getattr(sim, 'phase', 'insert') == 'flatten':
        status=f"{getattr(sim, 'flatten_visible', 0)}/{sim.n} reconstructed values"
        cur = getattr(sim, 'flatten_current', None)
        if cur is not None:
            status += f". current: src={cur[0]} value={cur[1]} → dest={cur[4]}"
    elif sim.last_event is None:
        status='simulate inserting values into ascending/descending subsequences'
    elif sim.done():
        status='simulated insertion complete'
    else:
        e=sim.last_event
        if e.get('tail_action') == 'replace':
            tail_msg = f" tail={e.get('old_tail')}→{e['value']}"
        else:
            tail_msg = f" tail=new {e['value']}"
        status=f"i={e['i']:02d} value={e['value']:02d} direction(i-1|i)={e['direction']}→game={e['game']} pile={e['pile']}{tail_msg}"
    draw_text(surface,small,status,48,66,MUTED)
    # legend
    x,y=860,30
    draw_text(surface,small,'Bottom marker:',x,y)
    r0=pygame.Rect(x,y+32,44,18); draw_marker(surface,r0,0); draw_text(surface,tiny,'game 0: descending piles',x+55,y+29,MUTED)
    r1=pygame.Rect(x,y+56,44,18); draw_marker(surface,r1,1); draw_text(surface,tiny,'game 1: ascending piles',x+55,y+53,MUTED)

def section_cell_geometry(n):
    usable_w = SECTION_W - BAR_GAP * (n - 1)
    cell_w = usable_w // n
    return SECTION_X, cell_w, BAR_GAP


def value_bar_rect(sim, index, section_y):
    """Return the full bar rect for a value bar in either equal-size section."""
    x0, cell_w, gap = section_cell_geometry(sim.n)
    value = sim.values[index]
    baseline = section_y + SECTION_H - 12
    h = max(BOTTOM_STRIP_H + 4, int(((value + VISUAL_HEIGHT_BOOST) / (sim.max_value + VISUAL_HEIGHT_BOOST)) * MAX_BAR_H))
    x = x0 + index * (cell_w + gap)
    return pygame.Rect(x, baseline - h, cell_w, h)


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
        game, pile = assignment
        main_color = pile_color(pile, game, COLOR_OFFSET)
        pygame.draw.rect(surface, main_color, main)
        draw_marker(surface, strip, game)

    draw_centered_value(surface, font, value, main, main_color)
    pygame.draw.rect(surface, OUTLINE, full, 1)
    if outline_current:
        pygame.draw.rect(surface, CURRENT_OUTLINE, full.inflate(8, 8), 4)


def draw_bars(surface, fonts, sim):
    _, small, tiny = fonts
    panel = pygame.Rect(48, TOP_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    display_message = 'unsorted input→simulation blueprint'
    if getattr(sim, 'phase', 'insert') == 'flatten':
        display_message = 'simulation blueprint→flat array'
    draw_text(surface, small, display_message, 64, TOP_SECTION_Y + 10, TEXT)

    flatten_cur = getattr(sim, 'flatten_current', None)
    for i in range(sim.n):
        is_current_insert = (getattr(sim, 'phase', 'insert') == 'insert' and i == sim.current_i)
        is_current_flatten_source = (getattr(sim, 'phase', 'insert') == 'flatten'
                                     and flatten_cur is not None
                                     and i == flatten_cur[0])
        draw_value_bar(surface, tiny, sim, i, TOP_SECTION_Y, sim.assignments[i],
                       outline_current=(is_current_insert or is_current_flatten_source))


def input_bar_rect(sim, index):
    """Return the full bar rect for the input bar at index."""
    return value_bar_rect(sim, index, TOP_SECTION_Y)


def build_flatten_order(sim):
    """
    Return the logical flattened array order after reconstruction.

    The reveal pass scans the simulated blueprint left-to-right, but game 1
    writes into its pile regions right-to-left. This function returns the final
    contiguous flattened order by destination slot, not by reveal order.
    """
    layout, _ = build_flatten_layout(sim)
    return [item for item in layout if item is not None]


def build_flatten_layout(sim):
    """
    Build both pieces needed for visualization:

    layout:
        Final flattened destination slots. Index k is the final displayed
        position in the middle row.

    reveal_events:
        Values encountered by scanning the blueprint/input left-to-right.
        Each event contains the destination slot that gets filled.

    Game 0 piles write right-to-left inside their destination region.
    Game 1 piles write left-to-right inside their destination region.
    The scan/gather order remains left-to-right for both games.
    """
    grouped_counts = {0: {}, 1: {}}
    for assignment in sim.assignments:
        if assignment is None:
            continue
        game, pile = assignment
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
        # Game 0 values are gathered left-to-right, but placed right-to-left.
        write_cursor[0][pile] = region_start[0][pile] + region_len[0][pile] - 1
    for pile in region_start[1]:
        write_cursor[1][pile] = region_start[1][pile]

    reveal_events = []
    for src_i, assignment in enumerate(sim.assignments):
        if assignment is None:
            continue
        game, pile = assignment
        value = sim.values[src_i]
        dest = write_cursor[game][pile]
        item = (src_i, value, game, pile, dest)
        layout[dest] = item
        reveal_events.append(item)

        if game == 0:
            write_cursor[0][pile] -= 1
        else:
            write_cursor[1][pile] += 1

    return layout, reveal_events

def draw_base_game_row(surface, fonts, sim, game, rect):
    _, small, tiny = fonts
    pygame.draw.rect(surface, (29, 32, 38), rect, border_radius=8)
    pygame.draw.rect(surface, (50, 54, 62), rect, 1, border_radius=8)

    label = 'game 0: descending piles | tails ascending' if game == 0 else 'game 1: ascending piles | tails descending'
    draw_text(surface, small, label, rect.x + 12, rect.y + 8, TEXT)
    draw_marker(surface, pygame.Rect(rect.right - 55, rect.y + 12, 36, 16), game)

    tails = sim.tails[game]
    if not tails:
        draw_text(surface, tiny, 'no piles yet', rect.x + 12, rect.y + 48, MUTED)
        return

    highlighted_tail = None
    if getattr(sim, 'phase', 'insert') == 'insert' and sim.last_event is not None:
        if sim.last_event.get('game') == game:
            highlighted_tail = sim.last_event.get('pile')

    gap = 6
    available_w = rect.w - 24 - gap * (len(tails) - 1)
    cell_w = max(24, min(58, available_w // len(tails)))
    total_w = len(tails) * cell_w + (len(tails) - 1) * gap
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


def draw_work_area_insert(surface, fonts, sim):
    _, small, tiny = fonts
    panel = pygame.Rect(48, WORK_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    draw_text(surface, small, 'pass 1: binary search over pile tails to find where to insert, update pile tails, update blueprint', 64, WORK_SECTION_Y + 10, TEXT)

    inner_x = 64
    inner_w = SCREEN_W - 128
    row_gap = 12
    row_y = WORK_SECTION_Y + 38
    row_h = (SECTION_H - 54 - row_gap) // 2
    game0_rect = pygame.Rect(inner_x, row_y, inner_w, row_h)
    game1_rect = pygame.Rect(inner_x, row_y + row_h + row_gap, inner_w, row_h)
    draw_base_game_row(surface, fonts, sim, 0, game0_rect)
    draw_base_game_row(surface, fonts, sim, 1, game1_rect)


def draw_work_area_flatten(surface, fonts, sim, visible_count):
    _, small, tiny = fonts
    panel = pygame.Rect(48, WORK_SECTION_Y, SCREEN_W - 96, SECTION_H)
    pygame.draw.rect(surface, PANEL_BG, panel, border_radius=8)
    draw_text(surface, small, 'pass 2: calculate placement location, flatten with scattered write', 64, WORK_SECTION_Y + 10, TEXT)
    draw_text(surface, tiny, 'scan left-to-right; game 0 writes R→L, game 1 writes L→R', 64, WORK_SECTION_Y + 34, MUTED)

    x0, cell_w, gap = section_cell_geometry(sim.n)
    guide_top = WORK_SECTION_Y + 52
    guide_h = SECTION_H - 64
    for k in range(sim.n):
        sx = x0 + k * (cell_w + gap)
        guide = pygame.Rect(sx, guide_top, cell_w, guide_h)
        pygame.draw.rect(surface, (52, 55, 64), guide, 1)

    _, reveal_events = build_flatten_layout(sim)
    baseline = WORK_SECTION_Y + SECTION_H - 12
    for src_i, value, game, pile, dest in reveal_events[:visible_count]:
        sx = x0 + dest * (cell_w + gap)
        bh = max(BOTTOM_STRIP_H + 4, int(((value + VISUAL_HEIGHT_BOOST) / (sim.max_value + VISUAL_HEIGHT_BOOST)) * MAX_BAR_H))
        full = pygame.Rect(sx, baseline - bh, cell_w, bh)
        main = pygame.Rect(sx, baseline - bh, cell_w, max(1, bh - BOTTOM_STRIP_H))
        bit = pygame.Rect(sx, baseline - BOTTOM_STRIP_H, cell_w, BOTTOM_STRIP_H)
        fill = pile_color(pile, game, COLOR_OFFSET)
        pygame.draw.rect(surface, fill, main)
        draw_marker(surface, bit, game)
        pygame.draw.rect(surface, OUTLINE, full, 1)
        draw_centered_value(surface, tiny, value, main, fill)

        cur = getattr(sim, 'flatten_current', None)
        if cur is not None and src_i == cur[0] and dest == cur[4]:
            pygame.draw.rect(surface, CURRENT_OUTLINE, full.inflate(8, 8), 4)


def draw_work_area(surface, fonts, sim):
    if getattr(sim, 'phase', 'insert') == 'flatten':
        draw_work_area_flatten(surface, fonts, sim, getattr(sim, 'flatten_visible', 0))
    else:
        draw_work_area_insert(surface, fonts, sim)



def draw_card_overlay(surface, fonts, title_text, summary_text):
    title, small, tiny = fonts
    card_w, card_h = 860, 180
    card = pygame.Rect((SCREEN_W - card_w) // 2, (SCREEN_H - card_h) // 2, card_w, card_h)
    shadow = card.move(6, 6)
    pygame.draw.rect(surface, (8, 9, 11), shadow, border_radius=14)
    pygame.draw.rect(surface, (245, 245, 245), card, border_radius=14)
    pygame.draw.rect(surface, CURRENT_OUTLINE, card, 4, border_radius=14)

    title_img = title.render(title_text, True, (18, 18, 18))
    summary_img = small.render(summary_text, True, (18, 18, 18))
    surface.blit(title_img, (card.x + (card.w - title_img.get_width()) // 2, card.y + 42))
    surface.blit(summary_img, (card.x + (card.w - summary_img.get_width()) // 2, card.y + 102))


def draw_intro_card_frame(surface, fonts, sim):
    draw_frame(surface, fonts, sim)
    draw_card_overlay(surface, fonts, INTRO_TITLE, INTRO_SUMMARY)


def draw_phase_card_frame(surface, fonts, sim, phase_num):
    draw_frame(surface, fonts, sim)
    draw_card_overlay(surface, fonts, f'phase {phase_num}', PHASE_SUMMARIES[phase_num])


def render_frames(surface, fonts, sim, frame_idx, frames):
    for _ in range(frames):
        draw_frame(surface, fonts, sim)
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


def draw_frame(surface, fonts, sim):
    surface.fill(BG)
    draw_header(surface, fonts, sim)
    draw_bars(surface, fonts, sim)
    draw_work_area(surface, fonts, sim)

# ================= AUDIO =================
def add_tone(data, sr, start_frame, value, game, kind='insert'):
    start = int((start_frame / FPS) * sr)
    norm = (value - 1) / max(1, NUM_INPUT_VALUES - 1)

    if kind == 'flatten':
        # Slightly shorter, cleaner tone for pass 2 so it is audible but distinct.
        dur = int(0.045 * sr)
        freq = (330 + norm * 770) * (0.94 if game == 0 else 1.10)
        volume = 0.18
        overtone = 0.18
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


def synth_audio(insert_events, flatten_events, total_frames):
    sr = 44100
    total_samples = int((total_frames / FPS) * sr) + sr // 2
    data = [0.0] * total_samples

    for step_idx, e in enumerate(insert_events):
        start_frame = INTRO_FRAMES + PHASE_CARD_FRAMES + step_idx * FRAMES_PER_STEP
        add_tone(data, sr, start_frame, e['value'], e['game'], kind='insert')

    flatten_start_frame = INTRO_FRAMES + PHASE_CARD_FRAMES + len(insert_events) * FRAMES_PER_STEP + SIM_HOLD_FRAMES + PHASE_CARD_FRAMES
    for step_idx, (src_i, value, game, pile, dest) in enumerate(flatten_events):
        start_frame = flatten_start_frame + step_idx * FRAMES_PER_STEP
        add_tone(data, sr, start_frame, value, game, kind='insert')

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

def main():
    pygame.init(); pygame.font.init()
    surface=pygame.Surface((SCREEN_W,SCREEN_H))
    fonts=(pygame.font.SysFont('consolas',28,bold=True), pygame.font.SysFont('consolas',18), pygame.font.SysFont('consolas',14))
    if FRAME_DIR.exists(): shutil.rmtree(FRAME_DIR)
    FRAME_DIR.mkdir(parents=True)
    sim=Sim(make_values())
    frame_idx=0
    # Intro card and phase 1 card.
    frame_idx = render_intro_card(surface, fonts, sim, frame_idx)
    frame_idx = render_phase_card(surface, fonts, sim, frame_idx, 1)

    # Process one value every FRAMES_PER_STEP frames.
    while not sim.done():
        print(f"sim step: {sim.current_i + 2}/{sim.n}")
        sim.step()
        frame_idx = render_frames(surface, fonts, sim, frame_idx, FRAMES_PER_STEP)
    # flatten hold
    frame_idx = render_frames(surface, fonts, sim, frame_idx, SIM_HOLD_FRAMES)

    # Phase 2 card, then replace the base-array work area with flattened reconstruction.
    frame_idx = render_phase_card(surface, fonts, sim, frame_idx, 2)
    sim.phase = 'flatten'
    sim.current_i = -1
    sim.flatten_visible = 0
    _, flatten_reveal_events = build_flatten_layout(sim)
    for visible in range(1, sim.n + 1):
        print(f"flatten step: {visible}/{sim.n}")
        sim.flatten_visible = visible
        sim.flatten_current = flatten_reveal_events[visible - 1]
        frame_idx = render_frames(surface, fonts, sim, frame_idx, FRAMES_PER_STEP)
    sim.flatten_current = None
    # flatten hold
    frame_idx = render_frames(surface, fonts, sim, frame_idx, FLATTEN_HOLD_FRAMES)
    # ending hold
    frame_idx = render_frames(surface, fonts, sim, frame_idx, END_FRAMES)
    print("adding audio")
    synth_audio(sim.events, flatten_reveal_events, frame_idx)
    with open(TRACE,'w') as f:
        f.write(f'values={sim.values}\n')
        f.write(f'NUM_INPUT_VALUES={NUM_INPUT_VALUES}\nVISUAL_HEIGHT_BOOST={VISUAL_HEIGHT_BOOST}\nFRAMES_PER_STEP={FRAMES_PER_STEP}\nINTRO_FRAMES={INTRO_FRAMES}\nPHASE_CARD_FRAMES={PHASE_CARD_FRAMES}\nBOTTOM_STRIP_H={BOTTOM_STRIP_H}\nTAIL_CELL_H={TAIL_CELL_H}\nINSERTION_TO_FLATTEN_HOLD_FRAMES={INSERTION_TO_FLATTEN_HOLD_FRAMES}\n')
        f.write('game 0 = descending piles / black marker with gray outline\n')
        f.write('game 1 = ascending piles / white marker / palette offset by 0.5 hue\n')
        f.write('insertion outlines the current input bar and the base-array tail cell updated by the insertion; replacement events record old_tail->new_tail\n')
        f.write('flattening outlines the source input bar during the second linear scan and the destination flattened bar\n')
        f.write('audio: flattening repeats the exact same tone logic as insertion for the same scanned values\n')
        f.write('layout: two equal visual sections; the work section shows base arrays during insertion, then switches to flattened bars during reconstruction; 45-frame intro and phase cards are shown\n')
        f.write('values sampled from 1 through NUM_INPUT_VALUES; the centered 8-value middle slice sorted into a natural run; bar heights use VISUAL_HEIGHT_BOOST only for rendering; numbers drawn on top and flattened bars\n')
        for e in sim.events:
            f.write(f"i={e['i']} value={e['value']} direction(i-1|i)={e['direction']}→game={e['game']} pile={e['pile']} tail_action={e['tail_action']} old_tail={e['old_tail']} tails0={e['tails0']} tails1={e['tails1']}\n")
        f.write(f'flatten_order={[(src_i, value, game, pile, dest) for (src_i, value, game, pile, dest) in build_flatten_order(sim)]}\n')
        layout, reveal_events = build_flatten_layout(sim)
        f.write(f'reveal_events={[(src_i, value, game, pile, dest) for (src_i, value, game, pile, dest) in reveal_events]}\n')
        f.write(f'total_frames={frame_idx}\n')
    cmd=['ffmpeg','-y','-r',str(FPS),'-i',str(FRAME_DIR/'frame_%05d.png'),'-i',str(OUT_WAV),'-c:v','libx264','-pix_fmt','yuv420p','-c:a','aac','-shortest',str(OUT_MP4)]
    ffmpeg_log = TRACE.with_suffix('.ffmpeg.log')
    with open(ffmpeg_log, 'w') as log:
        subprocess.run(cmd, check=True, stdout=log, stderr=log)
    # save script copy from current file if possible
    try:
        import inspect
        SCRIPT_COPY.write_text(Path(__file__).read_text())
    except Exception:
        pass
    print('WROTE', OUT_MP4)
    print('WROTE', TRACE)
    print('frames', frame_idx)
    print('values', sim.values)

if __name__=='__main__': main()
