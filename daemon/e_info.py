#!/usr/bin/python3

from PIL import Image, ImageDraw, ImageFont
import datetime
import json
import requests
import time

WIDTH = 176
HEIGHT = 264
PIXELS = 22
HPIXELS = PIXELS // 2
DIRECTORY = '/var/www/html/public/e_info'
FONT = '/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf'

class Text:
  def __init__(self, image, draw):
    self.image = image
    self.draw = draw
    self.font = ImageFont.truetype(FONT, size=PIXELS)
  def put(self, x, y, text, attr, half=False):
    fill = (attr & 0x01) + 1
    if (attr & 0x02):
      fill *= -1
    tmp_size = self.draw.textsize(text, self.font)
    if half:
      tmp_image = Image.new('P', tmp_size)
      tmp_draw = ImageDraw.Draw(tmp_image)
      if fill < 0:
        tmp_draw.rectangle((0, 0, tmp_size[0], tmp_size[1]), fill=-fill)
        fill = 0
      tmp_draw.text((0, 0), text, fill=fill, font=self.font)
      tmp_image = tmp_image.resize((tmp_size[0] // 2, tmp_size[1]))
      self.image.paste(tmp_image, (HPIXELS * x, PIXELS * y))
    else:
      if fill < 0:
        self.draw.rectangle((HPIXELS * x, PIXELS * y, HPIXELS * x + tmp_size[0], PIXELS * y + tmp_size[1]), fill=-fill)
        fill = 0
      self.draw.text((HPIXELS * x, PIXELS * y), text, fill=fill, font=self.font)

def extract(s, pfx, sfx):
  s = s[s.index(pfx) + len(pfx):]
  s = s[:s.index(sfx)]
  return s

def wget(url):
  DEBUG = False
  if DEBUG:
    try:
      return open('cache.' + url.replace('/', '_')).read()
    except:
      pass
  time.sleep(1.0)
  r = requests.get(url)
  page = r.text
  if DEBUG:
    open('cache.' + url.replace('/', '_'), 'w').write(page)
  return page

def get_weather():
  try:
    pass
  except Exception as e:
    result = ['−/−   -/  -', '−/−   -/  -']
  return result

def get_fxrate():
  try:
    pass
  except:
    result = ['USD/JPY   ?   ', 'EUR/JPY   ?   ']
  return result

def get_stock():
  result = []
  try:
    pass
  except:
    result.append(('N225 ', '     ?            '))
  try:
    pass
  except:
    result.append(('TPX  ', '     ?            '))
  try:
    pass
  except:
    result.append(('DJI  ', '     ?            '))
  return result

def main():
  # Initialize
  image = Image.new('P', (WIDTH, HEIGHT))
  image.putpalette([255, 255, 255, 0, 0, 0, 255, 0, 0])  # (white, blac, red)
  draw = ImageDraw.Draw(image)
  text = Text(image, draw)

  # Date
  now = datetime.datetime.now()
  mmdd = now.strftime('%m月%d日')
  wday = '月火水木金土日'[now.weekday()]
  text.put(0, 0, '%s(%s)' % (mmdd, wday), 2)

  # Weather
  text.put(0, 2, '天気', 3)
  weather = get_weather()
  for i, s in enumerate(weather):
    text.put(2, 3 + i, s, 0)
    text.put(15, 3 + i, '℃', 0, True)

  # FX rate
  text.put(0, 5, '為替', 3)
  fxrate = get_fxrate()
  for i, s in enumerate(fxrate):
    text.put(2, 6 + i, s, 0)

  # Stock
  text.put(0, 8, '株価', 3)
  stock = get_stock()
  for i, s in enumerate(stock):
    text.put(2, 9 + i, s[0], 0)
    text.put(7, 9 + i, s[1], 0, True)

  # Save images
  image.save(DIRECTORY + '/col.png')
  image.point(lambda x: int(x != 1), mode='1').save(DIRECTORY + '/blk.pbm')
  image.point(lambda x: int(x != 2), mode='1').save(DIRECTORY + '/red.pbm')

if __name__ == '__main__':
  main()
