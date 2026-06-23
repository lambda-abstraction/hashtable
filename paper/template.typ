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
  set par(leading: 1.5em, justify: true, first-line-indent: 1.25cm)

  show heading: set par(first-line-indent: 0pt)
  show heading.where(level: 1): it => {
    pagebreak(weak: true)
    set align(center)
    set text(weight: "bold", size: 14pt)
    upper(it.body)
    v(1em)
  }
  show heading.where(level: 2): it => {
    set align(center)
    set text(weight: "bold", size: 14pt)
    it.body
    v(0.5em)
  }
  show heading.where(level: 3): it => {
    set align(center)
    set text(weight: "bold", size: 14pt)
    it.body
    v(0.5em)
  }

  show figure.where(kind: image): it => {
    block(above: 6pt, below: 12pt)[
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

    #set text(weight: "bold")
    МИНИСТЕРСТВО НАУКИ И ВЫСШЕГО ОБРАЗОВАНИЯ РОССИЙСКОЙ ФЕДЕРАЦИИ \
    #set text(weight: "regular")
    Федеральное государственное образовательное учреждение высшего образования \
    «Санкт-Петербургский государственный морской технический университет» \
    ФАКУЛЬТЕТ ЦИФРОВЫХ ПРОМЫШЛЕННЫХ ТЕХНОЛОГИЙ \
    Кафедра Киберфизических систем

    #v(15%)
    #set text(size: 16pt, weight: "bold")
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

  outline(title: "Содержание", depth: 2, indent: auto)
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

