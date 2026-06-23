#let coursework(
  title: "",
  group: "",
  student: "",
  teacher: "",
  body,
) = {
  set page(
    paper: "a4",
    margin: (left: 30mm, right: 15mm, top: 20mm, bottom: 20mm),
    footer: context {
      set text(font: "Times New Roman", size: 14pt)
      align(center, counter(page).display())
    },
  )
  set text(font: "Times New Roman", size: 14pt, lang: "ru")
  set par(leading: 0.85em, spacing: 6pt, justify: true, first-line-indent: 1.25cm)

  show heading.where(level: 1): set heading(numbering: "1")
  show heading.where(level: 2): set heading(numbering: "1.1")
  show heading.where(level: 3): set heading(numbering: "1.1.1")

  show heading: it => {
    let is_special = it.body in ("Введение", "Заключение", "Содержание")
    set text(weight: "bold", size: 14pt)
    if it.level == 1 {
      pagebreak(weak: true)
      if is_special {
        set align(center)
        it.body
      } else {
        set align(left)
        set par(justify: true)
        it
      }
      v(1.5em)
    } else if it.level == 2 {
      if is_special {
        set align(center)
        it.body
      } else {
        set align(left)
        set par(justify: true)
        it
      }
      v(1em)
    } else if it.level == 3 {
      set align(left)
      set par(justify: true)
      it
      v(0.8em)
    }
  }

  show figure.where(kind: image): it => {
    block(above: 6pt, below: 30pt)[
      #align(center)[
        #it.body
        #v(6pt)
        #set par(leading: 1em)
        #it.supplement #it.counter.display() --- #it.caption.body
      ]
    ]
  }

  show figure.where(kind: "listing"): it => {
    set align(left)
    set par(first-line-indent: 0pt)
    block[
      #set par(leading: 1em)
      #it.supplement #it.counter.display() --- #it.caption.body
      #v(6pt)
      #block(above: 0pt, below: 0pt)[
        #rect(stroke: 1pt)[
          #set raw(block: true)
          #show raw: set text(font: "Courier New", size: 12pt)
          #set par(leading: 1em)
          #it.body
        ]
      ]
    ]
  }

  show table: set par(leading: 1.5em, spacing: 0pt)

  show figure.where(kind: "table"): it => {
    set align(left)
    par(first-line-indent: 0pt, leading: 1em)[
      #it.supplement #it.counter.display() --- #it.caption.body
    ]
    v(6pt)
    it.body
    v(12pt)
  }

  show math.equation.where(block: true): it => block(
    above: 12pt, below: 12pt,
    align(center, it)
  )

  page(footer: none)[
    #set par(
      leading: 0.6em,
      spacing: 0.6em,
      first-line-indent: 0pt,
      justify: false,
    )
    #set align(center)
    #set text(weight: "regular")
    МИНИСТЕРСТВО НАУКИ И ВЫСШЕГО ОБРАЗОВАНИЯ РОССИЙСКОЙ ФЕДЕРАЦИИ \
    Федеральное государственное образовательное учреждение высшего образования \
    «Санкт-Петербургский государственный морской технический университет» \
    ФАКУЛЬТЕТ ЦИФРОВЫХ ПРОМЫШЛЕННЫХ ТЕХНОЛОГИЙ \
    Кафедра Киберфизических систем

    #v(15%)
    #set text(size: 16pt, weight: "regular")
    Курсовая работа по теме: \
    #title \

    #v(20%)
    #grid(
      columns: (1.2fr, auto),
      [],
      [
        #set align(left)
        #set par(leading: 0.6em)
        Выполнил студент \
        группы: #group \
        #student \
        #v(1.5em)
        Проверил: \
        #teacher
      ],
    )

    #align(bottom + center)[
      Санкт-Петербург \
      2026
    ]
  ]

  heading(numbering: none, outlined: false)[Содержание]
  outline(title: none, depth: 2, indent: auto)

  body
}

#let code(body, caption: "", lang: "python", breakable: true) = {
  let content = if type(body) == "string" {
    raw(body, lang: lang, block: true)
  } else {
    body
  }
  figure(
    block(
      width: 100%,
      breakable: breakable,
      content,
    ),
    caption: caption,
    kind: "listing",
    supplement: "Листинг",
  )
}

#let fig(path, caption: "", width: 100%) = {
  figure(
    image(path, width: width),
    caption: caption,
    kind: "figure",
    supplement: "Рисунок",
  )
}
