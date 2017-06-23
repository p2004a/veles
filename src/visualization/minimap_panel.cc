/*
 * Copyright 2016 CodiLime
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <algorithm>
#include <cmath>
#include <functional>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpacerItem>

#include "visualization/minimap_panel.h"

namespace veles {
namespace visualization {

const float k_minimum_auto_selection_size = 0.1f;

typedef VisualizationMinimap::MinimapMode MinimapMode;

MinimapPanel::MinimapPanel(QWidget *parent, bool size_control) :
    QWidget(parent), size_control_(size_control), mode_(MinimapMode::VALUE),
    select_range_dialog_(new SelectRangeDialog(this)) {
  minimaps_.push_back(new VisualizationMinimap(size_control_, this));
  connect(minimaps_[0], &VisualizationMinimap::selectionChanged,
          std::bind(&MinimapPanel::updateSelection, this,
                    0, std::placeholders::_1,
                    std::placeholders::_2));
  connect(select_range_dialog_, &SelectRangeDialog::accepted, [this] () {
    selectRange(select_range_dialog_->getStartAddress(), select_range_dialog_->getEndAddress());
  });
  initLayout();
}

MinimapPanel::~MinimapPanel() {
}

void MinimapPanel::setSampler(util::ISampler *sampler) {
  sampler_ = sampler;
  while (minimaps_.size() > 1) {
    removeMinimap();
  }
  if (!minimap_samplers_.empty()) {
    delete minimap_samplers_[0];
    minimap_samplers_.pop_back();
  }
  minimap_samplers_.push_back(sampler_->clone());
  minimaps_[0]->setSampler(minimap_samplers_[0]);
  select_range_button_->setEnabled(!sampler_->empty());
  auto range = sampler_->getRange();
  selection_ = qMakePair(range.first, range.second);
}

QPair<size_t, size_t> MinimapPanel::getSelection() {
  return selection_;
}

/*****************************************************************************/
/* Private methods */
/*****************************************************************************/

void MinimapPanel::initLayout() {

  layout_ = new QVBoxLayout();

  minimaps_layout_ = new QHBoxLayout();
  minimaps_layout_->setContentsMargins(0, 0, 0, 0);
  for (auto it = minimaps_.begin(); it != minimaps_.end(); ++it) {
    minimaps_layout_->addWidget(*it, 1);
  }
  layout_->addLayout(minimaps_layout_);

  select_range_button_ = new QPushButton("select range");
  connect(select_range_button_, SIGNAL(released()),
          this, SLOT(showSelectRangeDialog()));

  if (size_control_) {
    layout_->addWidget(select_range_button_);
  }

  button_layout_ = new QHBoxLayout();
  remove_minimap_button_ = new QPushButton();
  remove_minimap_button_->setIcon(QIcon(":/images/minus.png"));
  remove_minimap_button_->setEnabled(false);
  connect(remove_minimap_button_, SIGNAL(released()),
          this, SLOT(removeMinimap()));
  if (size_control_) {
    button_layout_->addWidget(remove_minimap_button_, 0);
  }
  change_mode_button_ = new QPushButton("mode");
  connect(change_mode_button_, SIGNAL(released()),
          this, SLOT(changeMinimapMode()));
  button_layout_->addWidget(change_mode_button_);

  add_minimap_button_ = new QPushButton();
  add_minimap_button_->setIcon(QIcon(":/images/plus.png"));
  connect(add_minimap_button_, SIGNAL(released()), this, SLOT(addMinimap()));
  if (size_control_) {
    button_layout_->addWidget(add_minimap_button_, 0);
  }
  button_layout_->setSpacing(0);
  button_layout_->setContentsMargins(0, 0, 0, 0);

  layout_->addLayout(button_layout_);


  layout_->setSpacing(0);
  layout_->setContentsMargins(0, 0, 0, 0);
  setLayout(layout_);
}

VisualizationMinimap::MinimapColor MinimapPanel::getMinimapColor() {
  switch(mode_) {
  case MinimapMode::VALUE:
    return VisualizationMinimap::MinimapColor::GREEN;
  case MinimapMode::ENTROPY:
    return VisualizationMinimap::MinimapColor::RED;
  default:
    return VisualizationMinimap::MinimapColor::BLUE;
  }
}

/*****************************************************************************/
/* Slots */
/*****************************************************************************/

void MinimapPanel::addMinimap() {
  auto new_minimap = new VisualizationMinimap(size_control_, this);
  auto new_sampler = minimap_samplers_.back()->clone();
  auto range = minimaps_.back()->getSelectedRange();
  new_sampler->setRange(range.first, range.second);
  new_minimap->setSampler(new_sampler);
  new_minimap->setMinimapColor(getMinimapColor());
  new_minimap->setMinimapMode(mode_);
  connect(new_minimap, &VisualizationMinimap::selectionChanged,
          std::bind(&MinimapPanel::updateSelection, this,
                    minimaps_.length(), std::placeholders::_1,
                    std::placeholders::_2));
  minimap_samplers_.push_back(new_sampler);
  minimaps_.push_back(new_minimap);
  minimap_spacers_.push_back(new QSpacerItem(3, 0));
  minimaps_layout_->addItem(minimap_spacers_.back());
  minimaps_layout_->addWidget(new_minimap, 1);
  remove_minimap_button_->setEnabled(true);
}

void MinimapPanel::removeMinimap() {
  auto to_remove = minimaps_.back();
  auto to_remove_sampler = minimap_samplers_.back();
  minimaps_.pop_back();
  minimap_samplers_.pop_back();
  minimaps_layout_->removeWidget(to_remove);
  minimaps_layout_->removeItem(minimap_spacers_.back());
  auto to_remove_spacer = minimap_spacers_.back();
  minimap_spacers_.pop_back();
  delete to_remove_spacer;
  delete to_remove;
  delete to_remove_sampler;

  for (auto it = minimaps_.begin(); it != minimaps_.end(); ++it) {
    (*it)->refresh();
  }
  remove_minimap_button_->setEnabled(minimaps_.length() > 1);

  selection_ = minimaps_.back()->getSelectedRange();
  emit selectionChanged(selection_.first, selection_.second);
}

void MinimapPanel::changeMinimapMode() {
  mode_ = (mode_ == MinimapMode::VALUE) ? MinimapMode::ENTROPY : MinimapMode::VALUE;
  auto color = getMinimapColor();
  for (auto minimap : minimaps_) {
    minimap->setMinimapColor(color);
    minimap->setMinimapMode(mode_);
  }
}

void MinimapPanel::updateSelection(int minimap_index,
                                   size_t start, size_t end) {
  if (minimap_index == minimaps_.length() - 1) {
    selection_ = qMakePair(start, end);
    emit selectionChanged(start, end);
  } else {
    minimaps_[minimap_index + 1]->setRange(start, end, false);
  }
}

void MinimapPanel::showSelectRangeDialog() {
  if (select_range_dialog_->isVisible()) return;
  if (sampler_->empty()) return;
  auto min_address = sampler_->getFileOffset(0);
  auto max_address = sampler_->getFileOffset(sampler_->getSampleSize());
  select_range_dialog_->resetNumberFormat();
  select_range_dialog_->setRange(min_address, max_address);
  select_range_dialog_->show();
}

void MinimapPanel::selectRange(size_t start, size_t end) {
  size_t center = start + ((end - start) / 2);
  size_t size = end - start;
  size_t curr_start = 0;
  size_t curr_end = sampler_->getFileOffset(sampler_->getSampleSize());
  int index = 0;

  while (true) {
    if (index >= minimaps_.size()) {
      addMinimap();
    }
    size_t curr_size = std::max(size,
      static_cast<size_t>(std::ceil(k_minimum_auto_selection_size *
                          (curr_end - curr_start))));
    if (size == curr_size) {
      minimaps_[index]->setSelectedRange(start, end);
      break;
    }
    size_t dist_from_mid = curr_size / 2;
    size_t prev_start = curr_start;
    size_t prev_end = curr_end;

    if (dist_from_mid > center) {
      curr_start = prev_start;
    } else if (center - dist_from_mid < prev_start) {
      curr_start = prev_start;
    } else {
      curr_start = center - dist_from_mid;
    }

    curr_end = curr_start + curr_size;
    if (curr_end > prev_end) {
      curr_end = prev_end;
      curr_start = curr_end - curr_size;
    }

    // set selected range
    minimaps_[index]->setSelectedRange(curr_start, curr_end);
    index += 1;
  }

  while (minimaps_.size() > index + 1) {
    removeMinimap();
  }
}

void MinimapPanel::createMinimaps(size_t selection_size, int grow_factor) {
  size_t full_size = sampler_->getFileOffset(sampler_->getSampleSize());
  QVector<size_t> minimap_sizes;
  QVector<size_t> minimap_selection_sizes;
  size_t curr_size = selection_size;

  do {
    minimap_selection_sizes.append(std::min(full_size, curr_size));
    curr_size = std::min(static_cast<size_t>(curr_size * grow_factor), full_size);
    minimap_sizes.append(curr_size);
  } while (curr_size < full_size);

  int index = 0;
  while(index < minimap_sizes.size()) {
    if (index >= minimaps_.size()) {
      addMinimap();
    }
    minimaps_[index]->setRange(0, minimap_sizes[minimap_sizes.size() - index - 1]);
    minimaps_[index]->setSelectedRange(0, minimap_selection_sizes[minimap_selection_sizes.size() - index - 1]);
    ++index;
  }
  while (minimaps_.size() > index + 1) {
    removeMinimap();
  }
}

void MinimapPanel::scrollTo(size_t start) {
  for (int index = minimaps_.size() - 1; index >= 0; --index) {
    auto range = minimaps_[index]->getRange();
    auto selected_range = minimaps_[index]->getSelectedRange();

    if (selected_range.first == start) {
      break;
    }

    auto minimap_size = range.second - range.first;
    auto selection_size = selected_range.second - selected_range.first;
    size_t offset = start % (minimap_size - selection_size);
    size_t minimap_start = start - offset;
    minimaps_[index]->setRange(minimap_start, minimap_start + minimap_size);
    minimaps_[index]->setSelectedRange(minimap_start + offset, minimap_start + offset + selection_size);
    start = minimap_start;
  }
}

}  //  namespace visualization
}  //  namespace veles
