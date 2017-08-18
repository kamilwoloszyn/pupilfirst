class Coupon < ApplicationRecord
  belongs_to :referrer_startup, class_name: 'Startup', optional: true
  has_many :coupon_usages

  scope :referral, -> { where.not(referrer_startup_id: nil) }

  # TODO: Now that it's all referral coupons, probably scrap coupon_type entirely.
  TYPE_DISCOUNT = -'Discount'
  TYPE_MSP = -'Microsoft Student Partner'
  TYPE_REFERRAL = -'Referral'

  def self.valid_coupon_types
    [TYPE_DISCOUNT, TYPE_MSP, TYPE_REFERRAL]
  end

  validates :code, uniqueness: true, presence: true, length: { in: 4..10 }
  validates :coupon_type, inclusion: { in: valid_coupon_types }
  validates :referrer_startup_id, uniqueness: true, allow_nil: true
  validates :user_extension_days, presence: true, numericality: { only_integer: true, greater_than: 0, less_than_or_equal_to: 31 }
  validates :referrer_extension_days, presence: true, numericality: { only_integer: true, greater_than: 0, less_than_or_equal_to: 31 }, if: proc { |coupon| coupon.referrer_startup_id.present? }

  def still_valid?
    (expires_at.blank? || expires_at.future?) && redeems_left?
  end

  def redeems_left?
    return true if redeem_limit.zero?

    redeem_count = coupon_usages.redeemed.count
    redeem_count < redeem_limit
  end

  alias_attribute :name, :code

  # ransacker filter for admin index page
  ransacker :validity, formatter: proc { |v|
    coupons = if v == 'Valid'
      Coupon.all.select(&:still_valid?)
    elsif v == 'Invalid'
      Coupon.all.reject(&:still_valid?)
    else
      Coupon.all
    end

    coupons.present? ? coupons.map(&:id) : nil
  } do |parent|
    parent.table[:id]
  end
end
